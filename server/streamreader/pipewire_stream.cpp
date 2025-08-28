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
#include "pipewire_stream.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"

// 3rd party headers
#include <boost/asio/post.hpp>
#include <spa/debug/types.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/type-info.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>

// standard headers
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>

using namespace std;
using namespace std::chrono_literals;

namespace streamreader
{

static constexpr auto LOG_TAG = "PipeWireStream";

namespace
{
spa_audio_format sampleFormatToPipeWire(const SampleFormat& format)
{
    if (format.bits() == 8)
        return SPA_AUDIO_FORMAT_S8;
    else if (format.bits() == 16)
        return SPA_AUDIO_FORMAT_S16_LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 4))
        return SPA_AUDIO_FORMAT_S24_LE;
    else if (format.bits() == 32)
        return SPA_AUDIO_FORMAT_S32_LE;
    else
        throw SnapException("Unsupported sample format: " + cpt::to_string(format.bits()));
}
} // namespace

PipeWireStream::PipeWireStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, server_settings, uri), pw_main_loop_(nullptr), pw_context_(nullptr), pw_core_(nullptr), pw_stream_(nullptr), props_(nullptr),
      first_(true), silence_(0us), stream_state_(PW_STREAM_STATE_UNCONNECTED), running_(false)
{
    // Parse URI parameters
    target_device_ = uri_.getQuery("target", "");
    stream_name_ = uri_.getQuery("name", "Snapcast");
    send_silence_ = (uri_.getQuery("send_silence", "false") == "true");
    idle_threshold_ = std::chrono::milliseconds(std::max(cpt::stoi(uri_.getQuery("idle_threshold", "100")), 10));
    capture_sink_ = (uri_.getQuery("capture_sink", "false") == "true");

    // Initialize PipeWire
    pw_init(nullptr, nullptr);
}

PipeWireStream::~PipeWireStream()
{
    cleanup();
    pw_deinit();
}

void PipeWireStream::on_process(void* userdata)
{
    auto* stream = static_cast<PipeWireStream*>(userdata);
    stream->processAudio();
}

void PipeWireStream::on_state_changed(void* userdata, enum pw_stream_state old, enum pw_stream_state state, const char* error)
{
    auto* stream = static_cast<PipeWireStream*>(userdata);
    stream->stream_state_ = state;

    LOG(INFO, LOG_TAG) << "Stream state changed: " << pw_stream_state_as_string(old) << " -> " << pw_stream_state_as_string(state) << "\n";

    if (error)
    {
        LOG(ERROR, LOG_TAG) << "Stream error: " << error << "\n";
    }

    switch (state)
    {
        case PW_STREAM_STATE_ERROR:
        case PW_STREAM_STATE_UNCONNECTED:
            stream->running_ = false;
            if (stream->pw_main_loop_)
                pw_main_loop_quit(stream->pw_main_loop_);
            break;
        case PW_STREAM_STATE_STREAMING:
            LOG(INFO, LOG_TAG) << "Stream is now streaming\n";
            break;
        default:
            break;
    }
}

void PipeWireStream::on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param)
{
    std::ignore = userdata; // Unused for now

    if (param == nullptr || id != SPA_PARAM_Format)
        return;

    struct spa_audio_info info;
    spa_zero(info);

    if (spa_format_parse(param, &info.media_type, &info.media_subtype) < 0)
        return;

    if (info.media_type != SPA_MEDIA_TYPE_audio || info.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    if (spa_format_audio_raw_parse(param, &info.info.raw) < 0)
        return;

    LOG(INFO, LOG_TAG) << "Audio format: " << spa_debug_type_find_name(spa_type_audio_format, info.info.raw.format) << ", channels: " << info.info.raw.channels
                       << ", rate: " << info.info.raw.rate << "\n";
}

void PipeWireStream::processAudio()
{
    if (!running_)
        return;

    struct pw_buffer* b = pw_stream_dequeue_buffer(pw_stream_);
    if (b == nullptr)
    {
        LOG(WARNING, LOG_TAG) << "No buffer available\n";
        return;
    }

    struct spa_buffer* buf = b->buffer;
    struct spa_data* d = &buf->datas[0];

    if (d->data == nullptr)
    {
        LOG(WARNING, LOG_TAG) << "Buffer data is null\n";
        pw_stream_queue_buffer(pw_stream_, b);
        return;
    }

    uint32_t offset = std::min(d->chunk->offset, d->maxsize);
    uint32_t size = std::min(d->chunk->size, d->maxsize - offset);

    if (size == 0)
    {
        pw_stream_queue_buffer(pw_stream_, b);
        return;
    }

    uint8_t* data = static_cast<uint8_t*>(d->data) + offset;

    // Process data in chunks
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        temp_buffer_.insert(temp_buffer_.end(), data, data + size);

        // Process complete chunks
        while (temp_buffer_.size() >= chunk_->payloadSize)
        {
            // Copy data to chunk
            memcpy(chunk_->payload, temp_buffer_.data(), chunk_->payloadSize);
            temp_buffer_.erase(temp_buffer_.begin(), temp_buffer_.begin() + chunk_->payloadSize);

            // Check for silence
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

            // Send chunk if playing or sending silence
            if ((state_ == ReaderState::kPlaying) || ((state_ == ReaderState::kIdle) && send_silence_))
            {
                if (first_)
                {
                    first_ = false;
                    tvEncodedChunk_ = std::chrono::steady_clock::now() - chunk_->duration<std::chrono::nanoseconds>();
                }

                // Post to the strand to ensure thread safety
                boost::asio::post(strand_, [this, chunk = *chunk_]() mutable { chunkRead(chunk); });
            }
        }
    }

    pw_stream_queue_buffer(pw_stream_, b);
}

void PipeWireStream::on_core_info(void* userdata, const struct pw_core_info* info)
{
    std::ignore = userdata; // Unused
    LOG(DEBUG, LOG_TAG) << "Core info: " << info->name << " version " << info->version << "\n";
}

void PipeWireStream::on_core_error(void* userdata, uint32_t id, int seq, int res, const char* message)
{
    auto* stream = static_cast<PipeWireStream*>(userdata);

    LOG(ERROR, LOG_TAG) << "Core error: id=" << id << " seq=" << seq << " res=" << res << " (" << spa_strerror(res) << "): " << message << "\n";

    if (id == PW_ID_CORE && res == -EPIPE)
    {
        stream->running_ = false;
        if (stream->pw_main_loop_)
            pw_main_loop_quit(stream->pw_main_loop_);
    }
}

void PipeWireStream::start()
{
    LOG(DEBUG, LOG_TAG) << "Start, sampleformat: " << sampleFormat_.toString() << "\n";

    initPipeWire();
    first_ = true;
    tvEncodedChunk_ = std::chrono::steady_clock::now();
    running_ = true;
    PcmStream::start();

    // Run PipeWire main loop in a separate thread
    pw_thread_ = std::thread([this]() { pw_main_loop_run(pw_main_loop_); });
}

void PipeWireStream::cleanup()
{
    running_ = false;

    if (pw_main_loop_)
    {
        pw_main_loop_quit(pw_main_loop_);
    }

    if (pw_thread_.joinable())
    {
        pw_thread_.join();
    }

    PcmStream::stop();
    uninitPipeWire();
}

void PipeWireStream::stop()
{
    cleanup();
}

void PipeWireStream::initPipeWire()
{
    int res;
    std::array<const struct spa_pod*, 1> params;
    std::array<uint8_t, 1024> buffer;
    struct spa_pod_builder b;

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
    spa_pod_builder_init(&b, buffer.data(), buffer.size());
#pragma GCC diagnostic pop

    // Create main loop
    pw_main_loop_ = pw_main_loop_new(nullptr);
    if (!pw_main_loop_)
        throw SnapException("Failed to create PipeWire main loop");

    // Create context
    pw_context_ = pw_context_new(pw_main_loop_get_loop(pw_main_loop_), nullptr, 0);
    if (!pw_context_)
    {
        pw_main_loop_destroy(pw_main_loop_);
        pw_main_loop_ = nullptr;
        throw SnapException("Failed to create PipeWire context");
    }

    // Connect to PipeWire daemon
    pw_core_ = pw_context_connect(pw_context_, nullptr, 0);
    if (!pw_core_)
    {
        pw_context_destroy(pw_context_);
        pw_context_ = nullptr;
        pw_main_loop_destroy(pw_main_loop_);
        pw_main_loop_ = nullptr;
        throw SnapException("Failed to connect to PipeWire daemon");
    }

    // Set up core events
    spa_zero(core_events_);
    core_events_.version = PW_VERSION_CORE_EVENTS;
    core_events_.info = on_core_info;
    core_events_.error = on_core_error;
    pw_core_add_listener(pw_core_, &core_listener_, &core_events_, this);

    // Set up stream properties
    props_ = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Music", PW_KEY_APP_NAME, "Snapcast",
                               PW_KEY_MEDIA_CLASS, "Audio/Sink", PW_KEY_NODE_NAME, stream_name_.c_str(), nullptr);

    if (!target_device_.empty())
    {
        pw_properties_set(props_, PW_KEY_TARGET_OBJECT, target_device_.c_str());
    }

    if (capture_sink_)
    {
        pw_properties_set(props_, "stream.capture.sink", "true");
    }

    // Set latency
    pw_properties_setf(props_, PW_KEY_NODE_LATENCY, "%u/%u", static_cast<unsigned int>(chunk_ms_ * sampleFormat_.rate() / 1000), sampleFormat_.rate());

    // Create stream
    pw_stream_ = pw_stream_new(pw_core_, stream_name_.c_str(), props_);
    props_ = nullptr; // ownership transferred to stream

    if (!pw_stream_)
    {
        spa_hook_remove(&core_listener_);
        pw_core_disconnect(pw_core_);
        pw_core_ = nullptr;
        pw_context_destroy(pw_context_);
        pw_context_ = nullptr;
        pw_main_loop_destroy(pw_main_loop_);
        pw_main_loop_ = nullptr;
        throw SnapException("Failed to create PipeWire stream");
    }

    // Set up stream events
    spa_zero(stream_events_);
    stream_events_.version = PW_VERSION_STREAM_EVENTS;
    stream_events_.process = on_process;
    stream_events_.state_changed = on_state_changed;
    stream_events_.param_changed = on_param_changed;

    pw_stream_add_listener(pw_stream_, &stream_listener_, &stream_events_, this);

    // Set up audio format
    struct spa_audio_info_raw spa_audio_info = {};
    spa_audio_info.flags = SPA_AUDIO_FLAG_NONE;
    spa_audio_info.format = sampleFormatToPipeWire(sampleFormat_);
    spa_audio_info.rate = sampleFormat_.rate();
    spa_audio_info.channels = sampleFormat_.channels();

    // Set channel positions (stereo by default)
    if (sampleFormat_.channels() == 2)
    {
        spa_audio_info.position[0] = SPA_AUDIO_CHANNEL_FL;
        spa_audio_info.position[1] = SPA_AUDIO_CHANNEL_FR;
    }
    else if (sampleFormat_.channels() == 1)
    {
        spa_audio_info.position[0] = SPA_AUDIO_CHANNEL_MONO;
    }

    // Build format parameters
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &spa_audio_info);

    // Connect stream
    auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

    res = pw_stream_connect(pw_stream_, PW_DIRECTION_INPUT, PW_ID_ANY, flags, params.data(), params.size());

    if (res < 0)
    {
        spa_hook_remove(&stream_listener_);
        pw_stream_destroy(pw_stream_);
        pw_stream_ = nullptr;
        spa_hook_remove(&core_listener_);
        pw_core_disconnect(pw_core_);
        pw_core_ = nullptr;
        pw_context_destroy(pw_context_);
        pw_context_ = nullptr;
        pw_main_loop_destroy(pw_main_loop_);
        pw_main_loop_ = nullptr;
        throw SnapException("Failed to connect PipeWire stream: " + std::string(spa_strerror(res)));
    }

    // Clear temporary buffer
    temp_buffer_.clear();
    temp_buffer_.reserve(chunk_->payloadSize * 2);

    LOG(INFO, LOG_TAG) << "PipeWire stream initialized successfully\n";
}

void PipeWireStream::uninitPipeWire()
{
    if (pw_stream_)
    {
        spa_hook_remove(&stream_listener_);
        pw_stream_disconnect(pw_stream_);
        pw_stream_destroy(pw_stream_);
        pw_stream_ = nullptr;
    }

    if (pw_core_)
    {
        spa_hook_remove(&core_listener_);
        pw_core_disconnect(pw_core_);
        pw_core_ = nullptr;
    }

    if (pw_context_)
    {
        pw_context_destroy(pw_context_);
        pw_context_ = nullptr;
    }

    if (pw_main_loop_)
    {
        pw_main_loop_destroy(pw_main_loop_);
        pw_main_loop_ = nullptr;
    }
}

} // namespace streamreader
