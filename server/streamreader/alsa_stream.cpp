/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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

// 3rd party headers
#include <boost/asio/post.hpp>

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
    timer.async_wait(
        [handler = std::move(handler)](const boost::system::error_code& ec)
        {
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
}


void AlsaStream::start()
{
    LOG(DEBUG, LOG_TAG) << "Start, sampleformat: " << sampleFormat_.toString() << "\n";

    initAlsa();
    first_ = true;
    tvEncodedChunk_ = std::chrono::steady_clock::now();
    PcmStream::start();
    // wait(read_timer_, std::chrono::milliseconds(chunk_ms_), [this] { do_read(); });
    boost::asio::post(strand_, [this] { do_read(); });
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

#if 0 // Period size test code
    // snd_pcm_uframes_t period_size;
    // if ((err = snd_pcm_hw_params_get_period_size(hw_params, &period_size, nullptr)) < 0)
    //     LOG(ERROR, LOG_TAG) << "Can't get min period size: " << snd_strerror(err) << "\n";
    // else
    //     LOG(INFO, LOG_TAG) << "Period size: " << period_size << ", " << double(period_size) / double(sampleFormat_.rate()) * 1000. << " ms\n";

    // period_size = sampleFormat_.msRate() * chunk_ms_;
    // if ((err = snd_pcm_hw_params_set_period_size_near(handle_, hw_params, &period_size, 0)) < 0)
    //     LOG(ERROR, LOG_TAG) << "Can't set period size: " << snd_strerror(err) << "\n";
    // else
    //     LOG(INFO, LOG_TAG) << "Period size: " << period_size << ", " << double(period_size) / double(sampleFormat_.rate()) * 1000. << " ms\n";

    // chunk_ = std::make_unique<msg::PcmChunk>(sampleFormat_, 2*period_size, false);
    // LOG(INFO, LOG_TAG) << "Chunk duration: " << chunk_->duration<std::chrono::milliseconds>().count() << "\n";
#endif

    snd_pcm_hw_params_free(hw_params);

    if ((err = snd_pcm_prepare(handle_)) < 0)
        throw SnapException("Can't prepare audio interface for use: " + string(snd_strerror(err)));

    if (snd_pcm_state(handle_) == SND_PCM_STATE_PREPARED)
    {
        if ((err = snd_pcm_start(handle_)) < 0)
            throw SnapException("Failed to start PCM: " + string(snd_strerror(err)));
    }
}


void AlsaStream::uninitAlsa()
{
    if (handle_ != nullptr)
    {
        snd_pcm_close(handle_);
        handle_ = nullptr;
    }
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

        auto avail = snd_pcm_avail(handle_);
        if (avail >= 0)
        {
#if 0 // Some debug code
            static long max_avail = 0;
            if (avail > max_avail)
            {
                max_avail = avail;
                LOG(INFO, LOG_TAG) << "Max Available: " << avail << ", " << double(avail) / double(sampleFormat_.rate()) * 1000. << " ms\n";
            }
            static utils::logging::TimeConditional cond(1s);
            LOG(INFO, LOG_TAG) << cond << "Available: " << avail << ", " << double(avail) / double(sampleFormat_.rate()) * 1000. << " ms, max: " << double(max_avail) / double(sampleFormat_.rate()) * 1000. << " ms\n";
#endif

            // check if enough data is available to read from alsa
            if ((static_cast<int32_t>(chunk_->getFrameCount()) > avail))
            {
                // Calculate when there will be enough data available, add half chunk duration tolerance and try later
                auto available = std::chrono::milliseconds(static_cast<size_t>(double(avail) / double(sampleFormat_.rate()) * 1000.));
                auto missing = chunk_->duration<std::chrono::milliseconds>() - available;
                LOG(INFO, LOG_TAG) << "Not enough data available: " << available.count() << " ms, missing: " << missing.count()
                                   << " ms, needed: " << chunk_->duration<std::chrono::milliseconds>().count() << " ms\n";
                missing += chunk_->duration<std::chrono::milliseconds>() / 2;
                resync(missing);
                first_ = true;
                wait(read_timer_, missing, [this] { do_read(); });
                return;
            }
            // check if there is too much data available, i.e. if we are far behind
            else if (avail > static_cast<int32_t>(3 * chunk_->getFrameCount()))
            {
                // Fast forward, by reading and dropping audio frames
                // const auto newAvail = static_cast<int32_t>(chunk_->getFrameCount() + static_cast<uint32_t>(chunk_->format.msRate() * 20));
                const auto newAvail = 1.5 * chunk_->getFrameCount();
                LOG(INFO, LOG_TAG) << "Too many frames available, fast forwarding from " << avail << " frames ("
                                   << double(avail) / double(sampleFormat_.rate()) * 1000. << " ms) to " << newAvail << " frames ("
                                   << double(newAvail) / double(sampleFormat_.rate()) * 1000. << " ms)\n";
                do
                {
                    int count = snd_pcm_readi(handle_, chunk_->payload, std::min(chunk_->getFrameCount(), static_cast<uint32_t>(avail - newAvail)));
                    if (count <= 0)
                    {
                        // some read error happened, just break here, this will be handled properly later in the read loop
                        break;
                    }
                    avail -= count;
                    LOG(DEBUG, LOG_TAG) << "Read " << count << " frames (" << double(count) / double(sampleFormat_.rate()) * 1000.
                                        << " ms), available: " << avail << " frames (" << double(avail) / double(sampleFormat_.rate()) * 1000. << " ms)\n";
                } while (avail > newAvail);
                first_ = true;
            }
        }

        int toRead = chunk_->payloadSize;
        auto duration = chunk_->duration<std::chrono::nanoseconds>();
        int len = 0;
        do
        {
            snd_pcm_sframes_t count = snd_pcm_readi(handle_, chunk_->payload + len, (toRead - len) / chunk_->format.frameSize());
            if (count == -EAGAIN)
            {
                LOG(INFO, LOG_TAG) << "No data availabale, playing silence.\n";
                // no data available, fill with silence
                memset(chunk_->payload + len, 0, toRead - len);
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
            wait(read_timer_, next_read, [this] { do_read(); });
            return;
        }
        else if (next_read >= -kResyncTolerance)
        {
            LOG(INFO, LOG_TAG) << "next read < 0 (" << getName() << "): " << std::chrono::duration_cast<std::chrono::microseconds>(next_read).count() / 1000.
                               << " ms\n ";
            boost::asio::post(strand_, [this] { do_read(); });
        }
        else
        {
            // reading chunk_ms_ took longer than chunk_ms_
            resync(-next_read);
            first_ = true;
            boost::asio::post(strand_, [this] { do_read(); });
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
