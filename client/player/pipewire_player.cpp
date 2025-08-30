/***
    This file is part of snapcast
    Copyright (C) 2025  aanno <aannoaanno@gmail.com>

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
#include "pipewire_player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/time_defs.hpp"
#include "common/utils/logging.hpp"
#include "common/utils/string_utils.hpp"

// 3rd party headers
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

// standard headers
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;
using namespace std;

namespace player
{
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-statement-expression"
#pragma GCC diagnostic ignored "-Wc99-extensions"
#endif

static constexpr std::chrono::milliseconds BUFFER_TIME = 100ms;
static constexpr auto LOG_TAG = "PipeWirePlayer";

// Global device list for enumeration
static std::vector<PcmDevice> g_devices;
static std::mutex g_devices_mutex;

// C++11 compatible stream events initialization
struct pw_stream_events PipeWirePlayer::get_stream_events()
{
    struct pw_stream_events events = {};
    events.version = PW_VERSION_STREAM_EVENTS;
    events.destroy = nullptr;
    events.state_changed = on_state_changed;
    events.control_info = nullptr;
    events.io_changed = on_io_changed;
    events.param_changed = on_param_changed;
    events.add_buffer = nullptr;
    events.remove_buffer = nullptr;
    events.process = on_process;
    events.drained = on_drained;
    return events;
}

// C++11 compatible registry events initialization
struct pw_registry_events PipeWirePlayer::get_registry_events()
{
    struct pw_registry_events events = {};
    events.version = PW_VERSION_REGISTRY_EVENTS;
    events.global = registry_event_global;
    events.global_remove = registry_event_global_remove;
    return events;
}

std::vector<PcmDevice> PipeWirePlayer::pcm_list(const std::string& parameter)
{
    std::ignore = parameter;

    pw_init(nullptr, nullptr);

    // Create a threaded loop for device enumeration
    auto* loop = pw_thread_loop_new("snapcast-enum", nullptr);
    if (!loop)
        throw SnapException("Failed to create PipeWire thread loop");

    auto* context = pw_context_new(pw_thread_loop_get_loop(loop), nullptr, 0);
    if (!context)
    {
        pw_thread_loop_destroy(loop);
        throw SnapException("Failed to create PipeWire context");
    }

    pw_thread_loop_lock(loop);

    if (pw_thread_loop_start(loop) < 0)
    {
        pw_thread_loop_unlock(loop);
        pw_context_destroy(context);
        pw_thread_loop_destroy(loop);
        throw SnapException("Failed to start thread loop");
    }

    auto* core = pw_context_connect(context, nullptr, 0);
    if (!core)
    {
        pw_thread_loop_unlock(loop);
        pw_thread_loop_stop(loop);
        pw_context_destroy(context);
        pw_thread_loop_destroy(loop);
        throw SnapException("Failed to connect to PipeWire core");
    }

    auto* registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    if (!registry)
    {
        pw_core_disconnect(core);
        pw_thread_loop_unlock(loop);
        pw_thread_loop_stop(loop);
        pw_context_destroy(context);
        pw_thread_loop_destroy(loop);
        throw SnapException("Failed to get PipeWire registry");
    }

    {
        std::lock_guard<std::mutex> lock(g_devices_mutex);
        g_devices.clear();
    }

    // Add registry listener
    struct spa_hook registry_hook;
    auto registry_events = get_registry_events();
    pw_registry_add_listener(registry, &registry_hook, &registry_events, nullptr);

    // Let it run for a short time to enumerate devices
    pw_thread_loop_unlock(loop);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    pw_thread_loop_lock(loop);

    // Cleanup
    spa_hook_remove(&registry_hook);
    pw_proxy_destroy((struct pw_proxy*)registry);
    pw_core_disconnect(core);

    pw_thread_loop_unlock(loop);
    pw_thread_loop_stop(loop);
    pw_context_destroy(context);
    pw_thread_loop_destroy(loop);

    // Copy devices with mutex held
    std::vector<PcmDevice> devices;
    {
        std::lock_guard<std::mutex> lock(g_devices_mutex);
        devices = g_devices;
    }

    // Add default device
    devices.emplace(devices.begin(), -1, DEFAULT_DEVICE, "Let PipeWire choose the device");

    LOG(INFO, LOG_TAG) << "Found " << devices.size() << " audio devices\n";

    return devices;
}

void PipeWirePlayer::registry_event_global(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props)
{
    std::ignore = data;
    std::ignore = permissions;
    std::ignore = version;

    // Only process Node interfaces
    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
        return;

    const char* media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!media_class)
        return;

    // Only process Audio/Sink nodes
    if (strcmp(media_class, "Audio/Sink") != 0)
        return;

    const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
    const char* description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);

    if (name && description)
    {
        std::lock_guard<std::mutex> lock(g_devices_mutex);
        g_devices.emplace_back(id, name, description);
        LOG(DEBUG, LOG_TAG) << "Found audio sink: " << name << " (" << description << ")\n";
    }
}

void PipeWirePlayer::registry_event_global_remove(void* data, uint32_t id)
{
    std::ignore = data;
    std::ignore = id;
    // Remove device from list if needed
}

PipeWirePlayer::PipeWirePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, std::move(stream)), latency_(BUFFER_TIME), stream_ready_(false), connected_(false), last_chunk_tick_(0),
      disconnect_requested_(false), main_loop_(nullptr), context_(nullptr), core_(nullptr), pw_stream_(nullptr), registry_(nullptr),
      stream_events_(get_stream_events()), has_target_node_(false), target_node_(), node_id_(0), frame_size_(0), position_(nullptr)
{
    auto params = utils::string::split_pairs_to_container<std::vector<std::string>>(settings.parameter, ',', '=');

    if (params.find("buffer_time") != params.end())
        latency_ = std::chrono::milliseconds(std::max(cpt::stoi(params["buffer_time"].front()), 10));

    if (params.find("target") != params.end())
    {
        target_node_ = params["target"].front();
        has_target_node_ = true;
    }

    // Set default properties
    properties_[PW_KEY_MEDIA_TYPE] = "Audio";
    properties_[PW_KEY_MEDIA_CATEGORY] = "Playback";
    properties_[PW_KEY_MEDIA_ROLE] = "Music";
    properties_[PW_KEY_APP_NAME] = "Snapcast";
    properties_[PW_KEY_APP_ID] = "snapcast";
    properties_[PW_KEY_APP_ICON_NAME] = "snapcast";
    properties_[PW_KEY_NODE_NAME] = "Snapcast";
    properties_[PW_KEY_NODE_DESCRIPTION] = "Snapcast Audio Stream";

    // Calculate latency in samples
    const SampleFormat& format = stream_->getFormat();
    uint32_t latency_samples = (latency_.count() * format.rate()) / 1000;
    properties_[PW_KEY_NODE_LATENCY] = std::to_string(latency_samples) + "/" + std::to_string(format.rate());

    // Process custom properties
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

    LOG(INFO, LOG_TAG) << "Using buffer_time: " << latency_.count() / 1000 << " ms, target: " << (has_target_node_ ? target_node_ : "default") << "\n";
}

PipeWirePlayer::~PipeWirePlayer()
{
    LOG(DEBUG, LOG_TAG) << "Destructor\n";

    // Signal shutdown without calling stop()
    // This allows the player to be destroyed while still connecting
    active_ = false;

    // Signal the main loop to quit if it exists
    if (main_loop_)
    {
        pw_main_loop_quit(main_loop_);
    }

    // Wait for the worker thread to finish
    if (playerThread_.joinable())
    {
        playerThread_.join();
    }

    // Now clean up
    try
    {
        disconnect();
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception during cleanup: " << e.what() << "\n";
    }
}

bool PipeWirePlayer::needsThread() const
{
    return true;
}

void PipeWirePlayer::worker()
{
    LOG(DEBUG, LOG_TAG) << "Worker thread starting\n";

    try
    {
        // Initial connection
        connect();

        // Run the main loop until stop() is called
        while (active_)
        {
            if (main_loop_ && connected_)
            {
                LOG(DEBUG, LOG_TAG) << "Starting PipeWire main loop\n";
                LOG(DEBUG, LOG_TAG) << "main_loop_: " << main_loop_ << ", pw_stream_: " << pw_stream_ << ", connected_: " << connected_.load() << "\n";

                // Validate PipeWire objects before main loop
                if (!main_loop_)
                {
                    LOG(ERROR, LOG_TAG) << "main_loop_ is NULL!\n";
                    break;
                }
                if (!pw_stream_)
                {
                    LOG(ERROR, LOG_TAG) << "pw_stream_ is NULL!\n";
                    break;
                }

                auto stream_state = pw_stream_get_state(pw_stream_, nullptr);
                LOG(DEBUG, LOG_TAG) << "Stream state before main loop: " << pw_stream_state_as_string(stream_state) << "\n";

                LOG(DEBUG, LOG_TAG) << "About to call pw_main_loop_run...\n";

                // Start a monitoring thread to check for disconnect requests
                std::thread disconnect_monitor([this]()
                {
                    while (active_ && connected_)
                    {
                        if (disconnect_requested_.load(std::memory_order_acquire))
                        {
                            LOG(DEBUG, LOG_TAG) << "Disconnect monitor: quitting main loop\n";
                            if (main_loop_)
                                pw_main_loop_quit(main_loop_);
                            break;
                        }
                        std::this_thread::sleep_for(100ms);
                    }
                });

                // This will run until pw_main_loop_quit() is called
                int result = pw_main_loop_run(main_loop_);
                LOG(DEBUG, LOG_TAG) << "PipeWire main loop exited with result: " << result << "\n";

                // Clean up monitor thread
                if (disconnect_monitor.joinable())
                    disconnect_monitor.join();

                // Check if disconnection was requested due to silence
                if (disconnect_requested_.load(std::memory_order_acquire))
                {
                    LOG(INFO, LOG_TAG) << "Disconnecting from PipeWire due to silence.\n";
                    disconnect();
                    disconnect_requested_.store(false, std::memory_order_release);

                    // Wait for chunks to become available before reconnecting (matching PulseAudio pattern)
                    while (active_ && !stream_->waitForChunk(100ms))
                    {
                        static utils::logging::TimeConditional cond(2s);
                        LOG(DEBUG, LOG_TAG) << cond << "Waiting for a chunk to become available before reconnecting\n";
                    }

                    if (active_)
                    {
                        LOG(INFO, LOG_TAG) << "Chunk available, reconnecting to PipeWire\n";
                        try
                        {
                            connect();
                        }
                        catch (const std::exception& e)
                        {
                            LOG(ERROR, LOG_TAG) << "Failed to reconnect: " << e.what() << "\n";
                            // Will retry in next loop iteration
                        }
                    }
                }
                // If we get here and still active, it means there was an error
                else if (active_)
                {
                    LOG(ERROR, LOG_TAG) << "PipeWire main loop exited unexpectedly\n";

                    // Disconnect and wait a bit before reconnecting
                    disconnect();

                    // Wait before reconnecting, but check active_ flag
                    for (int i = 0; i < 10 && active_; ++i)
                    {
                        std::this_thread::sleep_for(100ms);
                    }

                    // Try to reconnect if still active
                    if (active_)
                    {
                        try
                        {
                            connect();
                        }
                        catch (const std::exception& e)
                        {
                            LOG(ERROR, LOG_TAG) << "Failed to reconnect: " << e.what() << "\n";
                            // Wait longer before next attempt
                            for (int i = 0; i < 50 && active_; ++i)
                            {
                                std::this_thread::sleep_for(100ms);
                            }
                        }
                    }
                }
            }
            else if (!connected_ && active_)
            {
                // Not connected but still active, try to connect
                try
                {
                    connect();
                }
                catch (const std::exception& e)
                {
                    if (active_)
                    {
                        LOG(ERROR, LOG_TAG) << "Failed to connect: " << e.what() << "\n";
                        // Wait before retry
                        for (int i = 0; i < 50 && active_; ++i)
                        {
                            std::this_thread::sleep_for(100ms);
                        }
                    }
                }
            }
            else
            {
                // Either not active or waiting for connection
                std::this_thread::sleep_for(100ms);
            }
        }

        LOG(DEBUG, LOG_TAG) << "Worker thread exiting normally\n";
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Fatal error in worker thread: " << e.what() << "\n";
        active_ = false;
    }
    catch (...)
    {
        LOG(ERROR, LOG_TAG) << "Unknown fatal error in worker thread\n";
        active_ = false;
    }

    // Ensure active_ is false when we exit
    active_ = false;
    LOG(DEBUG, LOG_TAG) << "Worker thread exited\n";
}

void PipeWirePlayer::start()
{
    LOG(INFO, LOG_TAG) << "Start\n";

    // pw_init already called in constructor - don't call again

    // The worker thread will handle the connection
    Player::start();
}

void PipeWirePlayer::connect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::array<uint8_t, 1024> buffer;
    std::array<const struct spa_pod*, 1> params;

    if (connected_)
    {
        LOG(WARNING, LOG_TAG) << "Already connected to PipeWire\n";
        return;
    }

    if (!active_)
    {
        LOG(DEBUG, LOG_TAG) << "Connect called during shutdown, ignoring\n";
        return;
    }

    LOG(INFO, LOG_TAG) << "Connecting to PipeWire\n";

    // Check if device exists (only for non-default devices)
    if (settings_.pcm_device.idx == -1 && settings_.pcm_device.name != DEFAULT_DEVICE)
    {
        LOG(WARNING, LOG_TAG) << "Device '" << settings_.pcm_device.name << "' not found, using default\n";
    }

    const SampleFormat& format = stream_->getFormat();

    // Set up audio format
    spa_zero(audio_info_);
    audio_info_.format = SPA_AUDIO_FORMAT_UNKNOWN;

    if (format.bits() == 8)
        audio_info_.format = SPA_AUDIO_FORMAT_U8;
    else if (format.bits() == 16)
        audio_info_.format = SPA_AUDIO_FORMAT_S16_LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 3))
        audio_info_.format = SPA_AUDIO_FORMAT_S24_LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 4))
        audio_info_.format = SPA_AUDIO_FORMAT_S24_32_LE;
    else if (format.bits() == 32)
        audio_info_.format = SPA_AUDIO_FORMAT_S32_LE;
    else
        throw SnapException("Unsupported sample format \"" + cpt::to_string(format.bits()) + "\"");

    audio_info_.channels = format.channels();
    audio_info_.rate = format.rate();
    frame_size_ = format.frameSize();

    // Create main loop
    main_loop_ = pw_main_loop_new(nullptr);
    if (!main_loop_)
        throw SnapException("Failed to create PipeWire main loop");

    // Create context
    context_ = pw_context_new(pw_main_loop_get_loop(main_loop_), nullptr, 0);
    if (!context_)
        throw SnapException("Failed to create PipeWire context");

    // Connect to core
    core_ = pw_context_connect(context_, nullptr, 0);
    if (!core_)
        throw SnapException("Failed to connect to PipeWire core");

    // Create stream properties
    struct pw_properties* props = pw_properties_new(nullptr, nullptr);
    for (const auto& property : properties_)
    {
        if (!property.second.empty())
            pw_properties_set(props, property.first.c_str(), property.second.c_str());
    }

    // Set target node if specified
    if (has_target_node_ && target_node_ != DEFAULT_DEVICE)
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, target_node_.c_str());
    else if (settings_.pcm_device.name != DEFAULT_DEVICE)
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, settings_.pcm_device.name.c_str());

    // Create playback stream
    pw_stream_ = pw_stream_new(core_, "Snapcast Playback", props);
    if (!pw_stream_)
        throw SnapException("Failed to create PipeWire stream");

    // Add stream listener - use member variable to avoid dangling pointer
    pw_stream_add_listener(pw_stream_, &stream_listener_, &stream_events_, this);

    // Create audio format parameters using spa_pod_builder
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());

    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info_);

    // Connect stream
    auto connect_flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                                      PW_STREAM_FLAG_RT_PROCESS); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    auto connect_ret = pw_stream_connect(pw_stream_, PW_DIRECTION_OUTPUT, PW_ID_ANY, connect_flags, params.data(), params.size());
    if (connect_ret < 0)
    {
        throw SnapException("Failed to connect PipeWire stream");
    }

    // Wait for stream to be ready
    stream_ready_ = false;
    auto wait_start = std::chrono::steady_clock::now();
    auto* loop = pw_main_loop_get_loop(main_loop_);

    while (!stream_ready_ && active_)
    {
        auto now = std::chrono::steady_clock::now();
        if (now - wait_start > 5s)
        {
            if (active_)
                throw SnapException("Timeout while waiting for PipeWire stream to become ready");
            else
                break; // Shutting down, exit gracefully
        }

        // Process events without blocking indefinitely
        pw_loop_iterate(loop, 10); // 10ms timeout
    }

    // Check if we're shutting down
    if (!active_)
    {
        LOG(DEBUG, LOG_TAG) << "Connect aborted due to shutdown\n";
        connected_ = false;
        return;
    }

    if (!stream_ready_)
        throw SnapException("Stream failed to become ready");

    connected_ = true;
    last_chunk_tick_ = chronos::getTickCount();
}

void PipeWirePlayer::stop()
{
    LOG(INFO, LOG_TAG) << "Stop\n";

    // Signal shutdown
    active_ = false;

    // Quit the main loop from outside the loop thread
    // This is safe and will cause pw_main_loop_run() to return
    if (main_loop_)
    {
        pw_main_loop_quit(main_loop_);
    }

    // Wait for the worker thread to finish
    // This ensures the main loop has exited
    Player::stop();

    // Now we can safely clean up
    disconnect();
}

void PipeWirePlayer::disconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!connected_)
        return;

    LOG(INFO, LOG_TAG) << "Disconnecting from PipeWire\n";

    connected_ = false;
    stream_ready_ = false;

    // Clean up in reverse order of creation
    if (pw_stream_)
    {
        spa_hook_remove(&stream_listener_);
        pw_stream_destroy(pw_stream_);
        pw_stream_ = nullptr;
    }

    if (core_)
    {
        pw_core_disconnect(core_);
        core_ = nullptr;
    }

    if (context_)
    {
        pw_context_destroy(context_);
        context_ = nullptr;
    }

    if (main_loop_)
    {
        pw_main_loop_destroy(main_loop_);
        main_loop_ = nullptr;
    }
}

void PipeWirePlayer::setHardwareVolume(const Volume& volume)
{
    if (!pw_stream_ || !stream_ready_)
        return;

    // Volume struct has volume in range [0..1] and mute flag
    auto vol = static_cast<float>(volume.volume);

    // If muted, set volume to 0
    if (volume.mute)
        vol = 0.0f;

    std::array<float, 2> values = {vol, vol}; // Same volume for both channels

    int ret = pw_stream_set_control(pw_stream_, SPA_PROP_channelVolumes, 2, values.data(), 0);

    if (ret >= 0)
    {
        LOG(DEBUG, LOG_TAG) << "Set hardware volume to " << (volume.volume * 100.0) << "% (mute: " << volume.mute << ")\n";

        // Update our internal volume state
        volume_ = volume;
    }
    else
    {
        LOG(ERROR, LOG_TAG) << "Failed to set hardware volume\n";
    }
}

bool PipeWirePlayer::getHardwareVolume(Volume& volume)
{
    if (!pw_stream_ || !stream_ready_)
        return false;

    // PipeWire doesn't have a standard way to get volume yet
    // For now, return the last set volume from the base class
    volume = volume_;
    return false; // Indicate we can't retrieve actual hardware volume
}

void PipeWirePlayer::on_state_changed(void* userdata, enum pw_stream_state old, enum pw_stream_state state, const char* error)
{
    if (!userdata)
        return;

    auto* self = static_cast<PipeWirePlayer*>(userdata);

    LOG(DEBUG, LOG_TAG) << "Stream state changed from " << pw_stream_state_as_string(old) << " to " << pw_stream_state_as_string(state);
    if (error)
        LOG(DEBUG, LOG_TAG) << " (error: " << error << ")";
    LOG(DEBUG, LOG_TAG) << "\n";

    switch (state)
    {
        case PW_STREAM_STATE_STREAMING:
            self->stream_ready_ = true;
            self->node_id_ = pw_stream_get_node_id(self->pw_stream_);
            LOG(INFO, LOG_TAG) << "Stream node " << self->node_id_ << " streaming\n";

            // Set initial volume if not muted and still active
            if (self->active_ && !self->volume_.mute && self->volume_.volume > 0.0)
            {
                self->setHardwareVolume(self->volume_);
            }
            break;

        case PW_STREAM_STATE_ERROR:
            LOG(ERROR, LOG_TAG) << "Stream error: " << (error ? error : "unknown") << "\n";
            self->stream_ready_ = false;
            // Exit the main loop on error so the worker thread can handle reconnection
            if (self->main_loop_ && self->active_.load(std::memory_order_acquire))
            {
                pw_main_loop_quit(self->main_loop_);
            }
            break;

        case PW_STREAM_STATE_UNCONNECTED:
            LOG(INFO, LOG_TAG) << "Stream disconnected\n";
            self->stream_ready_ = false;
            // Also exit main loop on disconnect, but only if still active
            if (self->main_loop_ && self->active_.load(std::memory_order_acquire))
            {
                pw_main_loop_quit(self->main_loop_);
            }
            break;

        case PW_STREAM_STATE_PAUSED:
            LOG(DEBUG, LOG_TAG) << "Stream paused\n";
            break;

        default:
            break;
    }
}

void PipeWirePlayer::on_process(void* userdata)
{
    if (!userdata)
        return;

    auto* self = static_cast<PipeWirePlayer*>(userdata);

    // Check if we're shutting down
    if (!self->active_.load(std::memory_order_acquire))
        return;

    if (!self->pw_stream_)
        return;

    struct pw_buffer* buffer = pw_stream_dequeue_buffer(self->pw_stream_);
    if (!buffer)
        return;

    struct spa_buffer* spa_buffer = buffer->buffer;
    if (!spa_buffer || !spa_buffer->datas)
    {
        pw_stream_queue_buffer(self->pw_stream_, buffer);
        return;
    }

    struct spa_data* d = &spa_buffer->datas[0];
    if (!d)
    {
        pw_stream_queue_buffer(self->pw_stream_, buffer);
        return;
    }

    if (!d->data)
    {
        pw_stream_queue_buffer(self->pw_stream_, buffer);
        return;
    }

    if (!d->chunk)
    {
        pw_stream_queue_buffer(self->pw_stream_, buffer);
        return;
    }

    // Calculate frames directly from maxsize (official PipeWire pattern - don't use chunk->offset)
    uint32_t stride = self->frame_size_;
    uint32_t n_frames = d->maxsize / stride;

    // Limit to requested frames if specified
    if (buffer->requested)
        n_frames = SPA_MIN(n_frames, buffer->requested);

    auto* p = static_cast<uint8_t*>(d->data);

    // Always produce audio even during shutdown to avoid underruns
    bool got_data = false;
    if (self->stream_ && self->active_.load(std::memory_order_acquire))
    {
        // Get accurate latency from PipeWire (similar to pa_stream_get_latency)
        struct pw_time time;
        auto latency_us = std::chrono::microseconds(0);

        if (pw_stream_get_time_n(self->pw_stream_, &time, sizeof(time)) == 0)
        {
            // Convert PipeWire timing to latency in microseconds
            // time.delay contains the total delay including buffering latency
            latency_us = std::chrono::microseconds(time.delay / 1000); // Convert nanoseconds to microseconds
        }
        else
        {
            // Fallback to buffer-based estimate if timing query fails
            latency_us = std::chrono::microseconds((n_frames * 1000000) / self->audio_info_.rate);
        }
        got_data = self->stream_->getPlayerChunkOrSilence(p, latency_us, n_frames);
    }

    if (!got_data)
    {
        // Fill with silence
        memset(p, 0, n_frames * stride);

        // Safe silence detection using atomic flag (matching PulseAudio pattern)
        auto now = chronos::getTickCount();
        if (now - self->last_chunk_tick_ > 5000) // 5 seconds silence (matching PulseAudio)
        {
            LOG(INFO, LOG_TAG) << "No chunk received for 5000ms, requesting disconnection from PipeWire.\n";
            // Set atomic flag to request disconnection in worker thread (safe approach)
            self->disconnect_requested_.store(true, std::memory_order_release);
            return;
        }
        else if (now - self->last_chunk_tick_ > 1000)
        {
            LOG(DEBUG, LOG_TAG) << "No audio data available, producing silence\n";
        }
    }
    else
    {
        self->last_chunk_tick_ = chronos::getTickCount();
        self->adjustVolume(reinterpret_cast<char*>(p), n_frames);
    }

    // Set chunk size (official PipeWire pattern - only set size, not offset/stride)
    if (d->chunk)
        d->chunk->size = n_frames * stride;

    pw_stream_queue_buffer(self->pw_stream_, buffer);
}

void PipeWirePlayer::on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param)
{
    if (!userdata)
        return;

    auto* self = static_cast<PipeWirePlayer*>(userdata);

    LOG(TRACE, LOG_TAG) << "Stream param changed: " << id << "\n";

    if (id != SPA_PARAM_Format || param == nullptr)
        return;

    struct spa_audio_info_raw info;
    spa_zero(info);

    if (spa_format_audio_raw_parse(param, &info) < 0)
        return;

    LOG(DEBUG, LOG_TAG) << "Format changed - rate: " << info.rate << ", channels: " << info.channels << "\n";

    std::ignore = self;
}

void PipeWirePlayer::on_io_changed(void* userdata, uint32_t id, void* area, uint32_t size)
{
    if (!userdata)
        return;

    auto* self = static_cast<PipeWirePlayer*>(userdata);
    std::ignore = size;

    switch (id)
    {
        case SPA_IO_Position:
            self->position_ = static_cast<struct spa_io_position*>(area);
            LOG(TRACE, LOG_TAG) << "Position IO changed\n";
            break;
        default:
            LOG(TRACE, LOG_TAG) << "IO changed: " << id << "\n";
            break;
    }
}

void PipeWirePlayer::on_drained(void* userdata)
{
    if (!userdata)
        return;

    auto* self = static_cast<PipeWirePlayer*>(userdata);
    LOG(DEBUG, LOG_TAG) << "Stream drained\n";
    std::ignore = self;
}

#ifdef __clang__
#pragma GCC diagnostic pop
#endif

} // namespace player
