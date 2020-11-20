/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#include <assert.h>
#include <iostream>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"
#include "pulse_player.hpp"

using namespace std;

static constexpr auto LOG_TAG = "PulsePlayer";
static constexpr auto kDefaultBuffer = 50ms;


PulsePlayer::PulsePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, stream), latency_(100000), pa_ml_(nullptr), pa_ctx_(nullptr), playstream_(nullptr)
{
}


PulsePlayer::~PulsePlayer()
{
    LOG(DEBUG, LOG_TAG) << "Destructor\n";
    stop();
}


bool PulsePlayer::needsThread() const
{
    return true;
}


void PulsePlayer::worker()
{
    // Run the mainloop until pa_mainloop_quit() is called
    // (this example never calls it, so the mainloop runs forever).
    pa_mainloop_run(pa_ml_, nullptr);
}


void PulsePlayer::underflowCallback(pa_stream* s)
{
    // We increase the latency by 50% if we get 6 underflows and latency is under 2s
    // This is very useful for over the network playback that can't handle low latencies
    underflows_++;
    LOG(INFO, LOG_TAG) << "undeflow #" << underflows_ << ", latency: " << latency_ / 1000 << " ms\n";
    if (underflows_ >= 6 && latency_ < 2000000)
    {
        latency_ = (latency_ * 3) / 2;
        bufattr_.maxlength = pa_usec_to_bytes(latency_, &ss_);
        bufattr_.tlength = pa_usec_to_bytes(latency_, &ss_);
        pa_stream_set_buffer_attr(s, &bufattr_, nullptr, nullptr);
        underflows_ = 0;
        LOG(INFO, LOG_TAG) << "latency increased to " << latency_ << " ms\n";
    }
}


void PulsePlayer::stateCallback(pa_context* c)
{
    pa_context_state_t state;
    state = pa_context_get_state(c);
    switch (state)
    {
            // These are just here for reference
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        default:
            break;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            pa_ready_ = 2;
            break;
        case PA_CONTEXT_READY:
            pa_ready_ = 1;
            break;
    }
}


void PulsePlayer::writeCallback(pa_stream* p, size_t nbytes)
{
    pa_usec_t usec;
    int neg;
    pa_stream_get_latency(p, &usec, &neg);

    auto rest = nbytes % stream_->getFormat().frameSize();
    if (rest != 0)
        LOG(INFO, LOG_TAG) << "Rest: " << rest << "\n";

    auto numFrames = nbytes / stream_->getFormat().frameSize();
    if (buffer_.size() < nbytes)
        buffer_.resize(nbytes);
    // LOG(TRACE, LOG_TAG) << "writeCallback latency " << usec << " us, frames: " << numFrames << "\n";
    if (!stream_->getPlayerChunk(buffer_.data(), std::chrono::microseconds(usec), numFrames))
    {
        LOG(INFO, LOG_TAG) << "Failed to get chunk. Playing silence.\n";
        memset(buffer_.data(), 0, numFrames);
    }
    else
    {
        adjustVolume(static_cast<char*>(buffer_.data()), numFrames);
    }

    pa_stream_write(p, buffer_.data(), nbytes, nullptr, 0LL, PA_SEEK_RELATIVE);
}


void PulsePlayer::start()
{
    // Create a mainloop API and connection to the default server
    pa_ml_ = pa_mainloop_new();
    pa_mainloop_api* pa_mlapi = pa_mainloop_get_api(pa_ml_);
    pa_ctx_ = pa_context_new(pa_mlapi, "Snapcast");
    pa_context_connect(pa_ctx_, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(
        pa_ctx_,
        [](pa_context* c, void* userdata) {
            auto self = static_cast<PulsePlayer*>(userdata);
            self->stateCallback(c);
        },
        this);

    // We can't do anything until PA is ready, so just iterate the mainloop
    // and continue
    while (pa_ready_ == 0)
        pa_mainloop_iterate(pa_ml_, 1, nullptr);

    if (pa_ready_ == 2)
        throw SnapException("PulseAudio is not ready");

    const SampleFormat& format = stream_->getFormat();
    ss_.rate = format.rate();
    ss_.channels = format.channels();
    if (format.bits() == 8)
        ss_.format = PA_SAMPLE_U8;
    else if (format.bits() == 16)
        ss_.format = PA_SAMPLE_S16LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 3))
        ss_.format = PA_SAMPLE_S24LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 4))
        ss_.format = PA_SAMPLE_S24_32LE;
    else if (format.bits() == 32)
        ss_.format = PA_SAMPLE_S32LE;
    else
        throw SnapException("Unsupported sample format: " + cpt::to_string(format.bits()));

    playstream_ = pa_stream_new(pa_ctx_, "Playback", &ss_, nullptr);
    if (!playstream_)
        throw SnapException("Failed to create PulseAudio stream");

    pa_stream_set_write_callback(
        playstream_,
        [](pa_stream* s, size_t length, void* userdata) {
            auto self = static_cast<PulsePlayer*>(userdata);
            self->writeCallback(s, length);
        },
        this);

    pa_stream_set_underflow_callback(
        playstream_,
        [](pa_stream* s, void* userdata) {
            auto self = static_cast<PulsePlayer*>(userdata);
            self->underflowCallback(s);
        },
        this);

    bufattr_.fragsize = (uint32_t)-1;
    bufattr_.maxlength = pa_usec_to_bytes(latency_, &ss_);
    bufattr_.minreq = pa_usec_to_bytes(0, &ss_);
    bufattr_.prebuf = (uint32_t)-1;
    bufattr_.tlength = pa_usec_to_bytes(latency_, &ss_);

    int result = pa_stream_connect_playback(
        playstream_, nullptr, &bufattr_, static_cast<pa_stream_flags>(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE),
        nullptr, nullptr);
    if (result < 0)
    {
        // Old pulse audio servers don't like the ADJUST_LATENCY flag, so retry without that
        result = pa_stream_connect_playback(playstream_, nullptr, &bufattr_,
                                            static_cast<pa_stream_flags>(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE), nullptr, nullptr);
    }
    if (result < 0)
        throw SnapException("Failed to connect PulseAudio playback stream");

    Player::start();
}


void PulsePlayer::stop()
{
    LOG(INFO, LOG_TAG) << "Stop\n";
    if (pa_ml_)
    {
        pa_mainloop_quit(pa_ml_, 0);
    }

    Player::stop();

    if (pa_ctx_)
    {
        pa_context_disconnect(pa_ctx_);
        pa_context_unref(pa_ctx_);
        pa_ctx_ = nullptr;
    }

    if (pa_ml_)
    {
        pa_mainloop_free(pa_ml_);
        pa_ml_ = nullptr;
    }

    if (playstream_)
    {
        pa_stream_set_state_callback(playstream_, nullptr, nullptr);
        pa_stream_set_read_callback(playstream_, nullptr, nullptr);
        pa_stream_set_underflow_callback(playstream_, nullptr, nullptr);
        pa_stream_set_overflow_callback(playstream_, nullptr, nullptr);

        pa_stream_disconnect(playstream_);
        pa_stream_unref(playstream_);
        playstream_ = nullptr;
    }
}
