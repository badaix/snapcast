/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/type-info.h>
#include <spa/utils/result.h>

// standard headers
#include <cerrno>
#include <cstring>
#include <memory>

using namespace std;
using namespace std::chrono_literals;

namespace streamreader
{

static constexpr auto LOG_TAG = "PipeWireStream";
static constexpr auto kResyncTolerance = 50ms;

namespace
{
template <typename Rep, typename Period>
void wait(boost::asio::steady_timer& timer, const std::chrono::duration<Rep, Period>& duration, std::function<void()> handler)
{
    timer.expires_after(duration);
    timer.async_wait([handler = std::move(handler)](const boost::system::error_code& ec)
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

spa_audio_format sampleFormatToPipeWire(const SampleFormat& format)
{
    if (format.bits() == 8)
        return SPA_AUDIO_FORMAT_S8;
    else if (format.bits() == 16)
        return SPA_AUDIO_FORMAT_S16_LE;
    else if ((format.bits() == 24) && (format.sampleSize() == 4))
        return SPA_AUDIO_FORMAT_S24_32_LE;
    else if (format.bits() == 32)
        return SPA_AUDIO_FORMAT_S32_LE;
    else
        throw SnapException("Unsupported sample format: " + cpt::to_string(format.bits()));
}
} // namespace

PipeWireStream::PipeWireStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, server_settings, uri), 
      pw_loop_(nullptr), 
      pw_stream_(nullptr), 
      props_(nullptr),
      pw_buffer_(nullptr),
      check_timer_(strand_), 
      silence_(0ms),
      stream_state_(PW_STREAM_STATE_UNCONNECTED),
      running_(false)
{
    // Parse URI parameters
    target_device_ = uri_.getQuery("device", "");
    stream_name_ = uri_.getQuery("name", "Snapcast");
    auto_connect_ = (uri_.getQuery("auto_connect", "true") == "true");
    send_silence_ = (uri_.getQuery("send_silence", "false") == "true");
    idle_threshold_ = std::chrono::milliseconds(std::max(cpt::stoi(uri_.getQuery("idle_threshold", "100")), 10));
    
    // Initialize PipeWire
    pw_init(nullptr, nullptr);
}

PipeWireStream::~PipeWireStream()
{
    stop();
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
    
    LOG(INFO, LOG_TAG) << "Stream state changed: " << pw_stream_state_as_string(old) 
                       << " -> " << pw_stream_state_as_string(state);
    
    if (error)
    {
        LOG(ERROR, LOG_TAG) << "Stream error: " << error << "\n";
    }
}

void PipeWireStream::on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param)
{
    auto* stream = static_cast<PipeWireStream*>(userdata);
    
    if (param == nullptr || id != SPA_PARAM_Format)
        return;
    
    struct spa_audio_info info;
    if (spa_format_parse(param, &info.media_type, &info.media_subtype) < 0)
        return;
    
    if (info.media_type != SPA_MEDIA_TYPE_audio ||
        info.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;
    
    spa_format_audio_raw_parse(param, &info.info.raw);
    
    LOG(INFO, LOG_TAG) << "Audio format: " << info.info.raw.format 
                       << ", channels: " << info.info.raw.channels
                       << ", rate: " << info.info.raw.rate << "\n";
}

void PipeWireStream::processAudio()
{
    if (!running_)
        return;
        
    pw_buffer_ = pw_stream_dequeue_buffer(pw_stream_);
    if (pw_buffer_ == nullptr)
    {
        LOG(WARNING, LOG_TAG) << "No buffer available\n";
        return;
    }
    
    struct spa_buffer* buf = pw_buffer_->buffer;
    struct spa_data* data = &buf->datas[0];
    
    if (data->data == nullptr)
    {
        LOG(WARNING, LOG_TAG) << "Buffer data is null\n";
        pw_stream_queue_buffer(pw_stream_, pw_buffer_);
        return;
    }
    
    size_t size = data->chunk->size;
    if (size == 0)
    {
        LOG(TRACE, LOG_TAG) << "Empty buffer\n";
        pw_stream_queue_buffer(pw_stream_, pw_buffer_);
        return;
    }
    
    // Copy data to temporary buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        temp_buffer_.insert(temp_buffer_.end(), 
                           static_cast<uint8_t*>(data->data), 
                           static_cast<uint8_t*>(data->data) + size);
    }
    
    pw_stream_queue_buffer(pw_stream_, pw_buffer_);
    
    // Process accumulated audio data if we have enough for a chunk
    boost::asio::post(strand_, [this] { checkSilence(); });
}

void PipeWireStream::checkSilence()
{
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    // Check if we have enough data for a complete chunk
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
            chunkRead(*chunk_);
        }
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
    
    // Start the PipeWire loop
    pw_thread_loop_start(pw_loop_);
}

void PipeWireStream::stop()
{
    running_ = false;
    PcmStream::stop();
    uninitPipeWire();
}

void PipeWireStream::initPipeWire()
{
    // Create thread loop
    pw_loop_ = pw_thread_loop_new("snapcast-capture", nullptr);
    if (!pw_loop_)
        throw SnapException("Failed to create PipeWire thread loop");
    
    // Set up stream properties
    props_ = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "Snapcast",
        PW_KEY_NODE_NAME, stream_name_.c_str(),
        nullptr);
    
    if (!target_device_.empty())
    {
        pw_properties_set(props_, PW_KEY_TARGET_OBJECT, target_device_.c_str());
    }
    
    if (!auto_connect_)
    {
        pw_properties_set(props_, PW_KEY_NODE_AUTOCONNECT, "false");
    }
    
    // Set up audio format
    spa_audio_info_ = {};
    spa_audio_info_.format = sampleFormatToPipeWire(sampleFormat_);
    spa_audio_info_.rate = sampleFormat_.rate();
    spa_audio_info_.channels = sampleFormat_.channels();
    
    // Set channel positions (stereo by default)
    if (sampleFormat_.channels() == 2)
    {
        spa_audio_info_.position[0] = SPA_AUDIO_CHANNEL_FL;
        spa_audio_info_.position[1] = SPA_AUDIO_CHANNEL_FR;
    }
    else if (sampleFormat_.channels() == 1)
    {
        spa_audio_info_.position[0] = SPA_AUDIO_CHANNEL_MONO;
    }
    
    // Create stream
    pw_stream_ = pw_stream_new_simple(
        pw_thread_loop_get_loop(pw_loop_),
        stream_name_.c_str(),
        props_,
        &stream_events_,
        this);
    
    if (!pw_stream_)
    {
        pw_thread_loop_destroy(pw_loop_);
        pw_loop_ = nullptr;
        throw SnapException("Failed to create PipeWire stream");
    }
    
    // Set up stream events
    stream_events_.version = PW_VERSION_STREAM_EVENTS;
    stream_events_.process = on_process;
    stream_events_.state_changed = on_state_changed;
    stream_events_.param_changed = on_param_changed;
    
    pw_stream_add_listener(pw_stream_, &stream_listener_, &stream_events_, this);
    
    // Build format parameters
    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    
    const struct spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &spa_audio_info_);
    
    // Connect stream
    int res = pw_stream_connect(
        pw_stream_,
        PW_DIRECTION_INPUT,
        PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | 
                                     PW_STREAM_FLAG_MAP_BUFFERS |
                                     PW_STREAM_FLAG_RT_PROCESS),
        params, 1);
    
    if (res < 0)
    {
        pw_stream_destroy(pw_stream_);
        pw_stream_ = nullptr;
        pw_thread_loop_destroy(pw_loop_);
        pw_loop_ = nullptr;
        throw SnapException("Failed to connect PipeWire stream: " + spa_strerror(res));
    }
    
    // Clear temporary buffer
    temp_buffer_.clear();
    buffer_offset_ = 0;
}

void PipeWireStream::uninitPipeWire()
{
    if (pw_loop_)
    {
        pw_thread_loop_stop(pw_loop_);
    }
    
    if (pw_stream_)
    {
        pw_stream_disconnect(pw_stream_);
        pw_stream_destroy(pw_stream_);
        pw_stream_ = nullptr;
    }
    
    if (pw_loop_)
    {
        pw_thread_loop_destroy(pw_loop_);
        pw_loop_ = nullptr;
    }
    
    if (props_)
    {
        pw_properties_free(props_);
        props_ = nullptr;
    }
}

} // namespace streamreader
