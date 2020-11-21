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

// Example code:
// https://code.qt.io/cgit/qt/qtmultimedia.git/tree/src/plugins/pulseaudio/qaudioinput_pulse.cpp?h=dev
// http://www.videolan.org/developers/vlc/modules/audio_output/pulse.c
// https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/Samples/AsyncPlayback/


PulsePlayer::PulsePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, stream), latency_(100ms), pa_ml_(nullptr), pa_ctx_(nullptr), playstream_(nullptr)
{
    auto params = utils::string::split_pairs(settings.parameter, ',', '=');
    if (params.find("latency") != params.end())
    {
        latency_ = std::chrono::milliseconds(std::max(cpt::stoi(params["latency"]), 10));
    }
    LOG(INFO, LOG_TAG) << "Using latency: " << latency_.count() / 1000 << " ms\n";
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
    pa_mainloop_run(pa_ml_, nullptr);
}


void PulsePlayer::setHardwareVolume(double volume, bool muted)
{
    last_change_ = std::chrono::steady_clock::now();
    pa_cvolume cvolume;
    if (muted)
        pa_cvolume_set(&cvolume, stream_->getFormat().channels(), PA_VOLUME_MUTED);
    else
        pa_cvolume_set(&cvolume, stream_->getFormat().channels(), volume * PA_VOLUME_NORM);
    pa_context_set_sink_input_volume(pa_ctx_, pa_stream_get_index(playstream_), &cvolume, nullptr, nullptr);
}


bool PulsePlayer::getHardwareVolume(double& volume, bool& muted)
{
    // This is called during start to send the initial volume to the server
    // Because getting the volume works async, we return false here
    // and instead trigger volume notification in pa_context_subscribe
    std::ignore = volume;
    std::ignore = muted;
    return false;
}


void PulsePlayer::triggerVolumeUpdate()
{
    pa_context_get_sink_input_info(pa_ctx_, pa_stream_get_index(playstream_),
                                   [](pa_context* ctx, const pa_sink_input_info* info, int eol, void* userdata) {
                                       std::ignore = ctx;
                                       LOG(DEBUG, LOG_TAG) << "pa_context_get_sink_info_by_index info: " << (info != nullptr) << ", eol: " << eol << "\n";
                                       if (info)
                                       {
                                           auto self = static_cast<PulsePlayer*>(userdata);
                                           auto volume = (double)pa_cvolume_avg(&(info->volume)) / (double)PA_VOLUME_NORM;
                                           bool muted = (info->mute != 0);
                                           LOG(DEBUG, LOG_TAG) << "volume changed: " << volume << ", muted: " << muted << "\n";

                                           auto now = std::chrono::steady_clock::now();
                                           if (now - self->last_change_ < 1s)
                                           {
                                               LOG(DEBUG, LOG_TAG) << "Last volume change by server: "
                                                                   << std::chrono::duration_cast<std::chrono::milliseconds>(now - self->last_change_).count()
                                                                   << " ms => ignoring volume change\n";
                                               return;
                                           }
                                           self->notifyVolumeChange(volume, muted);
                                       }
                                   },
                                   this);
}


void PulsePlayer::subscribeCallback(pa_context* ctx, pa_subscription_event_type_t event_type, uint32_t idx)
{
    std::ignore = ctx;
    LOG(TRACE, LOG_TAG) << "subscribeCallback, event type: " << event_type << ", idx: " << idx << "\n";
    unsigned facility = event_type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    event_type = static_cast<pa_subscription_event_type_t>(static_cast<int>(event_type) & PA_SUBSCRIPTION_EVENT_TYPE_MASK);
    if (facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT)
    {
        LOG(DEBUG, LOG_TAG) << "event_type: " << event_type << ", facility: " << facility << "\n";
        if (playstream_ && (idx == pa_stream_get_index(playstream_)))
            triggerVolumeUpdate();
    }
}


void PulsePlayer::underflowCallback(pa_stream* stream)
{
    // We increase the latency by 50% if we get 6 underflows and latency is under 2s
    // This is very useful for over the network playback that can't handle low latencies
    underflows_++;
    LOG(INFO, LOG_TAG) << "undeflow #" << underflows_ << ", latency: " << latency_.count() / 1000 << " ms\n";
    if (underflows_ >= 6 && latency_ < 500ms)
    {
        latency_ = (latency_ * 3) / 2;
        bufattr_.maxlength = pa_usec_to_bytes(latency_.count(), &pa_ss_);
        bufattr_.tlength = pa_usec_to_bytes(latency_.count(), &pa_ss_);
        pa_stream_set_buffer_attr(stream, &bufattr_, nullptr, nullptr);
        underflows_ = 0;
        LOG(INFO, LOG_TAG) << "latency increased to " << latency_.count() / 1000 << " ms\n";
    }
}


void PulsePlayer::stateCallback(pa_context* ctx)
{
    pa_context_state_t state = pa_context_get_state(ctx);
    string str_state = "unknown";
    pa_ready_ = 0;
    switch (state)
    {
        // These are just here for reference
        case PA_CONTEXT_UNCONNECTED:
            str_state = "unconnected";
            break;
        case PA_CONTEXT_CONNECTING:
            str_state = "connecting";
            break;
        case PA_CONTEXT_AUTHORIZING:
            str_state = "authorizing";
            break;
        case PA_CONTEXT_SETTING_NAME:
            str_state = "setting name";
            break;
        default:
            str_state = "unknown";
            break;
        case PA_CONTEXT_FAILED:
            str_state = "failed";
            pa_ready_ = 2;
            break;
        case PA_CONTEXT_TERMINATED:
            str_state = "terminated";
            pa_ready_ = 2;
            break;
        case PA_CONTEXT_READY:
            str_state = "ready";
            pa_ready_ = 1;
            break;
    }
    LOG(DEBUG, LOG_TAG) << "State changed " << state << ": " << str_state << "\n";
}


void PulsePlayer::writeCallback(pa_stream* stream, size_t nbytes)
{
    pa_usec_t usec;
    int neg;
    pa_stream_get_latency(stream, &usec, &neg);

    auto numFrames = nbytes / stream_->getFormat().frameSize();
    if (buffer_.size() < nbytes)
        buffer_.resize(nbytes);
    // LOG(TRACE, LOG_TAG) << "writeCallback latency " << usec << " us, frames: " << numFrames << "\n";
    if (!stream_->getPlayerChunk(buffer_.data(), std::chrono::microseconds(usec), numFrames))
    {
        // LOG(INFO, LOG_TAG) << "Failed to get chunk. Playing silence.\n";
        memset(buffer_.data(), 0, numFrames);
    }
    else
    {
        adjustVolume(static_cast<char*>(buffer_.data()), numFrames);
    }

    pa_stream_write(stream, buffer_.data(), nbytes, nullptr, 0LL, PA_SEEK_RELATIVE);
}


void PulsePlayer::start()
{
    const SampleFormat& format = stream_->getFormat();
    pa_ss_.rate = format.rate();
    pa_ss_.channels = format.channels();
    if (format.bits() == 8)
        pa_ss_.format = PA_SAMPLE_U8;
    else if (format.bits() == 16)
        pa_ss_.format = PA_SAMPLE_S16LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 3))
        pa_ss_.format = PA_SAMPLE_S24LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 4))
        pa_ss_.format = PA_SAMPLE_S24_32LE;
    else if (format.bits() == 32)
        pa_ss_.format = PA_SAMPLE_S32LE;
    else
        throw SnapException("Unsupported sample format: " + cpt::to_string(format.bits()));

    // Create a mainloop API and connection to the default server
    pa_ready_ = 0;
    pa_ml_ = pa_mainloop_new();
    pa_mainloop_api* pa_mlapi = pa_mainloop_get_api(pa_ml_);
    pa_ctx_ = pa_context_new(pa_mlapi, "Snapcast");
    pa_context_connect(pa_ctx_, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(pa_ctx_,
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

    playstream_ = pa_stream_new(pa_ctx_, "Playback", &pa_ss_, nullptr);
    if (!playstream_)
        throw SnapException("Failed to create PulseAudio stream");

    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::hardware)
    {
        pa_context_set_subscribe_callback(pa_ctx_,
                                          [](pa_context* ctx, pa_subscription_event_type_t event_type, uint32_t idx, void* userdata) {
                                              auto self = static_cast<PulsePlayer*>(userdata);
                                              self->subscribeCallback(ctx, event_type, idx);
                                          },
                                          this);
        const pa_subscription_mask_t mask = static_cast<pa_subscription_mask_t>(PA_SUBSCRIPTION_MASK_SINK_INPUT);

        pa_context_subscribe(pa_ctx_, mask,
                             [](pa_context* ctx, int success, void* userdata) {
                                 std::ignore = ctx;
                                 if (success)
                                 {
                                     auto self = static_cast<PulsePlayer*>(userdata);
                                     self->triggerVolumeUpdate();
                                 }
                             },
                             this);
    }

    pa_stream_set_write_callback(playstream_,
                                 [](pa_stream* stream, size_t length, void* userdata) {
                                     auto self = static_cast<PulsePlayer*>(userdata);
                                     self->writeCallback(stream, length);
                                 },
                                 this);

    pa_stream_set_underflow_callback(playstream_,
                                     [](pa_stream* stream, void* userdata) {
                                         auto self = static_cast<PulsePlayer*>(userdata);
                                         self->underflowCallback(stream);
                                     },
                                     this);

    bufattr_.fragsize = (uint32_t)-1;
    bufattr_.maxlength = pa_usec_to_bytes(latency_.count(), &pa_ss_);
    bufattr_.minreq = pa_usec_to_bytes(0, &pa_ss_);
    bufattr_.prebuf = (uint32_t)-1;
    bufattr_.tlength = pa_usec_to_bytes(latency_.count(), &pa_ss_);

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
