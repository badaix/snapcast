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
#include "pulse_player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/logging.hpp"
#include "common/utils/string_utils.hpp"

// 3rd party headers
#include <pulse/proplist.h>

// standard headers
#include <cassert>
#include <iostream>


using namespace std::chrono_literals;
using namespace std;

namespace player
{

static constexpr std::chrono::milliseconds BUFFER_TIME = 100ms;

static constexpr auto LOG_TAG = "PulsePlayer";

// Example code:
// https://code.qt.io/cgit/qt/qtmultimedia.git/tree/src/plugins/pulseaudio/qaudioinput_pulse.cpp?h=dev
// http://www.videolan.org/developers/vlc/modules/audio_output/pulse.c
// https://www.freedesktop.org/wiki/Software/PulseAudio/Documentation/Developer/Clients/Samples/AsyncPlayback/


vector<PcmDevice> PulsePlayer::pcm_list(const std::string& parameter)
{
    auto pa_ml = std::shared_ptr<pa_mainloop>(pa_mainloop_new(), [](pa_mainloop* pa_ml) { pa_mainloop_free(pa_ml); });
    pa_mainloop_api* pa_mlapi = pa_mainloop_get_api(pa_ml.get());
    auto pa_ctx = std::shared_ptr<pa_context>(pa_context_new(pa_mlapi, "Snapcast"),
                                              [](pa_context* pa_ctx)
                                              {
        pa_context_disconnect(pa_ctx);
        pa_context_unref(pa_ctx);
    });

    std::string pa_server;
    auto params = utils::string::split_pairs(parameter, ',', '=');
    if (params.find("server") != params.end())
        pa_server = params["server"];

    if (pa_context_connect(pa_ctx.get(), pa_server.empty() ? nullptr : pa_server.c_str(), PA_CONTEXT_NOFLAGS, nullptr) < 0)
        throw SnapException("Failed to connect to PulseAudio context, error: " + std::string(pa_strerror(pa_context_errno(pa_ctx.get()))));

    static int pa_ready = 0;
    pa_context_set_state_callback(
        pa_ctx.get(),
        [](pa_context* c, void* userdata)
        {
        std::ignore = userdata;
        pa_context_state_t state = pa_context_get_state(c);
        switch (state)
        {
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                pa_ready = 2;
                break;
            case PA_CONTEXT_READY:
                pa_ready = 1;
                break;
            default:
                break;
        }
        },
        nullptr);

    // We can't do anything until PA is ready, so just iterate the mainloop
    // and continue
    auto wait_start = std::chrono::steady_clock::now();
    while (pa_ready == 0)
    {
        auto now = std::chrono::steady_clock::now();
        if (now - wait_start > 5s)
            throw SnapException("Timeout while waiting for PulseAudio to become ready");
        if (pa_mainloop_iterate(pa_ml.get(), 1, nullptr) < 0)
            throw SnapException("Error while waiting for PulseAudio to become ready, error: " + std::string(pa_strerror(pa_context_errno(pa_ctx.get()))));
        this_thread::sleep_for(1ms);
    }

    if (pa_ready == 2)
        throw SnapException("PulseAudio context failed, error: " + std::string(pa_strerror(pa_context_errno(pa_ctx.get()))));

    static std::vector<PcmDevice> devices;
    auto* op = pa_context_get_sink_info_list(
        pa_ctx.get(),
        [](pa_context* ctx, const pa_sink_info* i, int eol, void* userdata) mutable
        {
        std::ignore = ctx;
        std::ignore = userdata;
        // auto self = static_cast<PulsePlayer*>(userdata);
        // If eol is set to a positive number, you're at the end of the list
        if (eol <= 0)
            devices.emplace_back(i->index, i->name, i->description);
        },
        nullptr);

    if (op == nullptr)
        throw SnapException("PulseAudio get sink info list failed, error: " + std::string(pa_strerror(pa_context_errno(pa_ctx.get()))));

    wait_start = std::chrono::steady_clock::now();

    while (pa_operation_get_state(op) != PA_OPERATION_DONE)
    {
        if (pa_operation_get_state(op) == PA_OPERATION_CANCELED)
            throw SnapException("PulseAudio operation canceled");

        auto now = std::chrono::steady_clock::now();
        if (now - wait_start > 2s)
            break;
        pa_mainloop_iterate(pa_ml.get(), 1, nullptr);
    }
    int max_idx = -1;
    for (const auto& device : devices)
        max_idx = std::max(max_idx, device.idx);

    devices.emplace(devices.begin(), max_idx + 1, DEFAULT_DEVICE, "Let PulseAudio server choose the device");
    return devices;
}


PulsePlayer::PulsePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, stream), latency_(BUFFER_TIME), pa_ml_(nullptr), pa_ctx_(nullptr), playstream_(nullptr), proplist_(nullptr),
      server_(std::nullopt)
{
    auto params = utils::string::split_pairs_to_container<std::vector<std::string>>(settings.parameter, ',', '=');
    if (params.find("buffer_time") != params.end())
        latency_ = std::chrono::milliseconds(std::max(cpt::stoi(params["buffer_time"].front()), 10));
    if (params.find("server") != params.end())
        server_ = params["server"].front();
    properties_[PA_PROP_MEDIA_ROLE] = "music";
    properties_[PA_PROP_APPLICATION_ICON_NAME] = "snapcast";
    if (params.find("property") != params.end())
    {
        for (const auto& p : params["property"])
        {
            std::string value;
            std::string key = utils::string::split_left(p, '=', value);
            if (!key.empty())
                properties_[key] = value;
        }
    }
    for (const auto& property : properties_)
    {
        if (!property.second.empty())
            LOG(INFO, LOG_TAG) << "Setting property \"" << property.first << "\" to \"" << property.second << "\"\n";
    }

    LOG(INFO, LOG_TAG) << "Using buffer_time: " << latency_.count() / 1000 << " ms, server: " << server_.value_or("default") << "\n";
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
    while (active_)
    {
        pa_mainloop_run(pa_ml_, nullptr);

        // if we are still active, wait for a chunk and attempt to reconnect
        while (active_ && !stream_->waitForChunk(100ms))
        {
            static utils::logging::TimeConditional cond(2s);
            LOG(DEBUG, LOG_TAG) << cond << "Waiting for a chunk to become available before reconnecting\n";
        }

        while (active_)
        {
            LOG(INFO, LOG_TAG) << "Chunk available, reconnecting to pulse\n";
            try
            {
                connect();
                break;
            }
            catch (const std::exception& e)
            {
                LOG(ERROR, LOG_TAG) << "Exception while connecting to pulse: " << e.what() << endl;
                disconnect();
                chronos::sleep(100);
            }
        }
    }
}

void PulsePlayer::setHardwareVolume(double volume, bool muted)
{
    // if we're not connected to pulse, ignore the hardware volume change as the volume will be set upon reconnect
    if (playstream_ == nullptr)
        return;

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
    pa_context_get_sink_input_info(
        pa_ctx_, pa_stream_get_index(playstream_),
        [](pa_context* ctx, const pa_sink_input_info* info, int eol, void* userdata)
        {
        std::ignore = ctx;
        LOG(DEBUG, LOG_TAG) << "pa_context_get_sink_info_by_index info: " << (info != nullptr) << ", eol: " << eol << "\n";
        if (info != nullptr)
        {
            auto* self = static_cast<PulsePlayer*>(userdata);
            self->volume_ = static_cast<double>(pa_cvolume_avg(&(info->volume))) / static_cast<double>(PA_VOLUME_NORM);
            self->muted_ = (info->mute != 0);
            LOG(DEBUG, LOG_TAG) << "Volume changed: " << self->volume_ << ", muted: " << self->muted_ << "\n";

            auto now = std::chrono::steady_clock::now();
            if (now - self->last_change_ < 1s)
            {
                LOG(DEBUG, LOG_TAG) << "Last volume change by server: "
                                    << std::chrono::duration_cast<std::chrono::milliseconds>(now - self->last_change_).count()
                                    << " ms => ignoring volume change\n";
                return;
            }
            self->notifyVolumeChange(self->volume_, self->muted_);
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
        if ((playstream_ != nullptr) && (idx == pa_stream_get_index(playstream_)))
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
    if (!stream_->getPlayerChunkOrSilence(buffer_.data(), std::chrono::microseconds(usec), numFrames))
    {
        // if we haven't got a chunk for a while, it's time to disconnect from pulse so the sound device
        // can become idle/suspended.
        if (chronos::getTickCount() - last_chunk_tick_ > 5000)
        {
            LOG(INFO, LOG_TAG) << "No chunk received for 5000ms, disconnecting from pulse.\n";
            this->disconnect();
            return;
        }
        // LOG(TRACE, LOG_TAG) << "Failed to get chunk. Playing silence.\n";
    }
    else
    {
        last_chunk_tick_ = chronos::getTickCount();
        adjustVolume(static_cast<char*>(buffer_.data()), numFrames);
    }

    pa_stream_write(stream, buffer_.data(), nbytes, nullptr, 0LL, PA_SEEK_RELATIVE);
}

void PulsePlayer::start()
{
    LOG(INFO, LOG_TAG) << "Start\n";

    this->connect();
    Player::start();
}

void PulsePlayer::connect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    LOG(INFO, LOG_TAG) << "Connecting to pulse\n";

    if (settings_.pcm_device.idx == -1)
        throw SnapException("Can't open " + settings_.pcm_device.name + ", error: No such device");

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
        throw SnapException("Unsupported sample format \"" + cpt::to_string(format.bits()) + "\"");

    // Create a mainloop API and connection to the default server
    pa_ready_ = 0;
    pa_ml_ = pa_mainloop_new();
    pa_mainloop_api* pa_mlapi = pa_mainloop_get_api(pa_ml_);

    proplist_ = pa_proplist_new();
    for (const auto& property : properties_)
    {
        if (!property.second.empty())
            pa_proplist_sets(proplist_, property.first.c_str(), property.second.c_str());
    }

    pa_ctx_ = pa_context_new_with_proplist(pa_mlapi, "Snapcast", proplist_);

    const char* server = server_.has_value() ? server_.value().c_str() : nullptr;
    if (pa_context_connect(pa_ctx_, server, PA_CONTEXT_NOFLAGS, nullptr) < 0)
        throw SnapException("Failed to connect to PulseAudio context, error: " + std::string(pa_strerror(pa_context_errno(pa_ctx_))));

    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(
        pa_ctx_,
        [](pa_context* c, void* userdata)
        {
        auto* self = static_cast<PulsePlayer*>(userdata);
        self->stateCallback(c);
        },
        this);

    // We can't do anything until PA is ready, so just iterate the mainloop
    // and continue
    auto wait_start = std::chrono::steady_clock::now();
    while (pa_ready_ == 0)
    {
        auto now = std::chrono::steady_clock::now();
        if (now - wait_start > 5s)
            throw SnapException("Timeout while waiting for PulseAudio to become ready");
        if (pa_mainloop_iterate(pa_ml_, 1, nullptr) < 0)
            throw SnapException("Error while waiting for PulseAudio to become ready, error: " + std::string(pa_strerror(pa_context_errno(pa_ctx_))));
        this_thread::sleep_for(1ms);
    }

    if (pa_ready_ == 2)
        throw SnapException("PulseAudio is not ready, error: " + std::string(pa_strerror(pa_context_errno(pa_ctx_))));

    playstream_ = pa_stream_new(pa_ctx_, "Playback", &pa_ss_, nullptr);
    if (playstream_ == nullptr)
        throw SnapException("Failed to create PulseAudio stream");

    if (settings_.mixer.mode == ClientSettings::Mixer::Mode::hardware)
    {
        pa_context_set_subscribe_callback(
            pa_ctx_,
            [](pa_context* ctx, pa_subscription_event_type_t event_type, uint32_t idx, void* userdata)
            {
            auto* self = static_cast<PulsePlayer*>(userdata);
            self->subscribeCallback(ctx, event_type, idx);
            },
            this);
        const auto mask = static_cast<pa_subscription_mask_t>(PA_SUBSCRIPTION_MASK_SINK_INPUT);

        pa_context_subscribe(
            pa_ctx_, mask,
            [](pa_context* ctx, int success, void* userdata)
            {
            std::ignore = ctx;
            if (success != 0)
            {
                auto* self = static_cast<PulsePlayer*>(userdata);
                self->triggerVolumeUpdate();
            }
            },
            this);
    }

    pa_stream_set_write_callback(
        playstream_,
        [](pa_stream* stream, size_t length, void* userdata)
        {
        auto* self = static_cast<PulsePlayer*>(userdata);
        self->writeCallback(stream, length);
        },
        this);

    pa_stream_set_underflow_callback(
        playstream_,
        [](pa_stream* stream, void* userdata)
        {
        auto* self = static_cast<PulsePlayer*>(userdata);
        self->underflowCallback(stream);
        },
        this);

    bufattr_.fragsize = pa_usec_to_bytes(latency_.count(), &pa_ss_);
    bufattr_.maxlength = pa_usec_to_bytes(latency_.count(), &pa_ss_);
    bufattr_.minreq = static_cast<uint32_t>(-1);
    bufattr_.prebuf = static_cast<uint32_t>(-1);
    bufattr_.tlength = pa_usec_to_bytes(latency_.count(), &pa_ss_);

    const char* device = nullptr;
    if (settings_.pcm_device.name != DEFAULT_DEVICE)
        device = settings_.pcm_device.name.c_str();

    int result = pa_stream_connect_playback(
        playstream_, device, &bufattr_, static_cast<pa_stream_flags>(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE),
        nullptr, nullptr);
    if (result < 0)
    {
        // Old pulse audio servers don't like the ADJUST_LATENCY flag, so retry without that
        result = pa_stream_connect_playback(playstream_, device, &bufattr_,
                                            static_cast<pa_stream_flags>(PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE), nullptr, nullptr);
    }
    if (result < 0)
        throw SnapException("Failed to connect PulseAudio playback stream");

    // we just connected, pretend we got a chunk so we don't immediately disconnect.
    last_chunk_tick_ = chronos::getTickCount();
}

void PulsePlayer::stop()
{
    LOG(INFO, LOG_TAG) << "Stop\n";

    this->disconnect();
    Player::stop();
}

void PulsePlayer::disconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    LOG(INFO, LOG_TAG) << "Disconnecting from pulse\n";

    if (pa_ml_ != nullptr)
    {
        pa_mainloop_quit(pa_ml_, 0);
    }

    if (pa_ctx_ != nullptr)
    {
        pa_context_disconnect(pa_ctx_);
        pa_context_unref(pa_ctx_);
        pa_ctx_ = nullptr;
    }

    if (pa_ml_ != nullptr)
    {
        pa_mainloop_free(pa_ml_);
        pa_ml_ = nullptr;
    }

    if (playstream_ != nullptr)
    {
        pa_stream_set_state_callback(playstream_, nullptr, nullptr);
        pa_stream_set_read_callback(playstream_, nullptr, nullptr);
        pa_stream_set_underflow_callback(playstream_, nullptr, nullptr);
        pa_stream_set_overflow_callback(playstream_, nullptr, nullptr);

        pa_stream_disconnect(playstream_);
        pa_stream_unref(playstream_);
        playstream_ = nullptr;
    }

    if (proplist_ != nullptr)
    {
        pa_proplist_free(proplist_);
        proplist_ = nullptr;
    }
}

} // namespace player
