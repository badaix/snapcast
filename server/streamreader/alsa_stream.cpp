/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

// prototype/interface header file
#include "alsa_stream.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"

// standard headers
#include <cerrno>
#include <memory>


using namespace std;
using namespace std::chrono_literals;

namespace streamreader
{

static constexpr auto LOG_TAG = "AlsaStream";
static constexpr auto kResyncTolerance = 50ms;

// https://superuser.com/questions/597227/linux-arecord-capture-sound-card-output-rather-than-microphone-input
// https://wiki.ubuntuusers.de/.asoundrc/
// https://alsa.opensrc.org/Dsnoop#The_dsnoop_howto
// https://linuxconfig.org/how-to-test-microphone-with-audio-linux-sound-architecture-alsa
// https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2latency_8c-example.html#a30


namespace
{
template <typename Rep, typename Period>
void wait(boost::asio::steady_timer& timer, const std::chrono::duration<Rep, Period>& duration, std::function<void()> handler)
{
    timer.expires_after(duration);
    timer.async_wait([handler = std::move(handler)](const boost::system::error_code& ec) {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error during async wait: " << ec.message() << "\n";
        }
        else
        {
            handler();
        }
    });
}
} // namespace


AlsaStream::AlsaStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, server_settings, uri), handle_(nullptr), read_timer_(strand_), silence_(0ms)
{
    device_ = uri_.getQuery("device", "hw:0");
    send_silence_ = (uri_.getQuery("send_silence", "false") == "true");
    idle_threshold_ = std::chrono::milliseconds(std::max(cpt::stoi(uri_.getQuery("idle_threshold", "100")), 10));
    double silence_threshold_percent = 0.;
    try
    {
        silence_threshold_percent = cpt::stod(uri_.getQuery("silence_threshold_percent", "0"));
    }
    catch (...)
    {
    }
    int32_t max_amplitude = std::pow(2, sampleFormat_.bits() - 1) - 1;
    silence_threshold_ = max_amplitude * (silence_threshold_percent / 100.);
    LOG(DEBUG, LOG_TAG) << "Device: " << device_ << ", silence threshold percent: " << silence_threshold_percent
                        << ", silence threshold amplitude: " << silence_threshold_ << "\n";
}


void AlsaStream::start()
{
    LOG(DEBUG, LOG_TAG) << "Start, sampleformat: " << sampleFormat_.toString() << "\n";

    // idle_bytes_ = 0;
    // max_idle_bytes_ = sampleFormat_.rate() * sampleFormat_.frameSize() * dryout_ms_ / 1000;

    initAlsa();
    chunk_ = std::make_unique<msg::PcmChunk>(sampleFormat_, chunk_ms_);
    silent_chunk_ = std::vector<char>(chunk_->payloadSize, 0);
    LOG(DEBUG, LOG_TAG) << "Chunk duration: " << chunk_->durationMs() << " ms, frames: " << chunk_->getFrameCount() << ", size: " << chunk_->payloadSize
                        << "\n";
    first_ = true;
    tvEncodedChunk_ = std::chrono::steady_clock::now();
    PcmStream::start();
    // wait(read_timer_, std::chrono::milliseconds(chunk_ms_), [this] { do_read(); });
    do_read();
}


void AlsaStream::stop()
{
    PcmStream::stop();
    uninitAlsa();
}


void AlsaStream::initAlsa()
{
    int err;
    unsigned int rate = sampleFormat_.rate();
    snd_pcm_format_t snd_pcm_format;
    if (sampleFormat_.bits() == 8)
        snd_pcm_format = SND_PCM_FORMAT_S8;
    else if (sampleFormat_.bits() == 16)
        snd_pcm_format = SND_PCM_FORMAT_S16_LE;
    else if ((sampleFormat_.bits() == 24) && (sampleFormat_.sampleSize() == 4))
        snd_pcm_format = SND_PCM_FORMAT_S24_LE;
    else if (sampleFormat_.bits() == 32)
        snd_pcm_format = SND_PCM_FORMAT_S32_LE;
    else
        throw SnapException("Unsupported sample format: " + cpt::to_string(sampleFormat_.bits()));

    if ((err = snd_pcm_open(&handle_, device_.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK)) < 0) // SND_PCM_NONBLOCK
        throw SnapException("Can't open device '" + device_ + "', error: " + snd_strerror(err));

    snd_pcm_hw_params_t* hw_params;
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
        throw SnapException("Can't allocate hardware parameter structure: " + string(snd_strerror(err)));

    if ((err = snd_pcm_hw_params_any(handle_, hw_params)) < 0)
        throw SnapException("Can't fill params: " + string(snd_strerror(err)));

    if ((err = snd_pcm_hw_params_set_access(handle_, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        throw SnapException("Can't set interleaved mode: " + string(snd_strerror(err)));

    if ((err = snd_pcm_hw_params_set_format(handle_, hw_params, snd_pcm_format)) < 0)
        throw SnapException("Can't set sample format: " + string(snd_strerror(err)));

    if ((err = snd_pcm_hw_params_set_rate_near(handle_, hw_params, &rate, nullptr)) < 0)
        throw SnapException("Can't set rate: " + string(snd_strerror(err)));

    if (rate != sampleFormat_.rate())
    {
        LOG(WARNING, LOG_TAG) << "Rate is not accurate (requested: " << sampleFormat_.rate() << ", got: " << rate << "), using: " << rate << "\n";
        sampleFormat_.setFormat(rate, sampleFormat_.bits(), sampleFormat_.channels());
    }

    if ((err = snd_pcm_hw_params_set_channels(handle_, hw_params, sampleFormat_.channels())) < 0)
        throw SnapException("Can't set channel count: " + string(snd_strerror(err)));

    if ((err = snd_pcm_hw_params(handle_, hw_params)) < 0)
        throw SnapException("Can't set hardware parameters: " + string(snd_strerror(err)));

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(handle_)) < 0)
        throw SnapException("Can't prepare audio interface for use: " + string(snd_strerror(err)));
}


void AlsaStream::uninitAlsa()
{
    if (handle_ != nullptr)
    {
        snd_pcm_close(handle_);
        handle_ = nullptr;
    }
}


bool AlsaStream::isSilent(const msg::PcmChunk& chunk) const
{
    if (silence_threshold_ == 0)
        return (std::memcmp(chunk.payload, silent_chunk_.data(), silent_chunk_.size()) == 0);

    if (sampleFormat_.sampleSize() == 1)
    {
        auto payload = chunk.getPayload<int8_t>();
        for (size_t n = 0; n < payload.second; ++n)
        {
            if (abs(payload.first[n]) > silence_threshold_)
                return false;
        }
    }
    else if (sampleFormat_.sampleSize() == 2)
    {
        auto payload = chunk.getPayload<int16_t>();
        for (size_t n = 0; n < payload.second; ++n)
        {
            if (abs(payload.first[n]) > silence_threshold_)
                return false;
        }
    }
    else if (sampleFormat_.sampleSize() == 4)
    {
        auto payload = chunk.getPayload<int32_t>();
        for (size_t n = 0; n < payload.second; ++n)
        {
            if (abs(payload.first[n]) > silence_threshold_)
                return false;
        }
    }
    return true;
}

void AlsaStream::do_read()
{
    try
    {
        if (first_)
        {
            LOG(TRACE, LOG_TAG) << "First read, initializing nextTick to now\n";
            nextTick_ = std::chrono::steady_clock::now();
        }

        int toRead = chunk_->payloadSize;
        auto duration = chunk_->duration<std::chrono::nanoseconds>();
        int len = 0;
        do
        {
            int count = snd_pcm_readi(handle_, chunk_->payload + len, (toRead - len) / chunk_->format.frameSize());
            if (count == -EAGAIN)
            {
                LOG(INFO, LOG_TAG) << "No data availabale, playing silence.\n";
                // no data available, fill with silence
                memset(chunk_->payload + len, 0, toRead - len);
                // idle_bytes_ += toRead - len;
                break;
            }
            else if (count == 0)
            {
                throw SnapException("end of file");
            }
            else if (count < 0)
            {
                // ESTRPIPE
                LOG(ERROR, LOG_TAG) << "Error reading PCM data: " << snd_strerror(count) << " (code: " << count << ")\n";
                first_ = true;
                uninitAlsa();
                initAlsa();
                continue;
            }
            else
            {
                // LOG(TRACE, LOG_TAG) << "count: " << count << ", len: " << len << ", toRead: " << toRead << "\n";
                len += count * chunk_->format.frameSize();
            }
        } while (len < toRead);

        if (isSilent(*chunk_))
        {
            silence_ += chunk_->duration<std::chrono::microseconds>();
            if (silence_ > idle_threshold_)
            {
                setState(ReaderState::kIdle);
            }
        }
        else
        {
            silence_ = 0ms;
            if ((state_ == ReaderState::kIdle) && !send_silence_)
                first_ = true;
            setState(ReaderState::kPlaying);
        }

        // LOG(DEBUG, LOG_TAG) << "Received " << len << "/" << toRead << " bytes\n";
        if (first_)
        {
            first_ = false;
            // initialize the stream's base timestamp to now minus the chunk's duration
            tvEncodedChunk_ = std::chrono::steady_clock::now() - duration;
        }

        if ((state_ == ReaderState::kPlaying) || ((state_ == ReaderState::kIdle) && send_silence_))
        {
            chunkRead(*chunk_);
        }

        nextTick_ += duration;
        auto currentTick = std::chrono::steady_clock::now();
        auto next_read = nextTick_ - currentTick;
        if (next_read >= 0ms)
        {
            // LOG(DEBUG, LOG_TAG) << "Next read: " << std::chrono::duration_cast<std::chrono::milliseconds>(next_read).count() << "\n";
            // synchronize reads to an interval of chunk_ms_
            wait(read_timer_, nextTick_ - currentTick, [this] { do_read(); });
            return;
        }
        else if (next_read >= -kResyncTolerance)
        {
            LOG(INFO, LOG_TAG) << "next read < 0 (" << getName() << "): " << std::chrono::duration_cast<std::chrono::microseconds>(next_read).count() / 1000.
                               << " ms\n ";
            do_read();
        }
        else
        {
            // reading chunk_ms_ took longer than chunk_ms_
            resync(-next_read);
            first_ = true;
            wait(read_timer_, nextTick_ - currentTick, [this] { do_read(); });
        }

        lastException_ = "";
    }
    catch (const std::exception& e)
    {
        if (lastException_ != e.what())
        {
            LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << std::endl;
            lastException_ = e.what();
        }
        first_ = true;
        uninitAlsa();
        initAlsa();
        wait(read_timer_, 100ms, [this] { do_read(); });
    }
}

} // namespace streamreader
