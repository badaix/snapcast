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

#pragma once

// local headers
#include "pcm_stream.hpp"

// 3rd party headers
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <memory>
#include <mutex>

namespace streamreader
{

/// Reads and decodes PCM data from a PipeWire audio stream
/**
 * Reads PCM from a PipeWire audio stream and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmStream::Listener
 */
class PipeWireStream : public PcmStream
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    PipeWireStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);
    ~PipeWireStream() override;

    void start() override;
    void stop() override;

private:
    // PipeWire callbacks
    static void on_process(void* userdata);
    static void on_state_changed(void* userdata, enum pw_stream_state old, enum pw_stream_state state, const char* error);
    static void on_param_changed(void* userdata, uint32_t id, const struct spa_pod* param);
    
    void initPipeWire();
    void uninitPipeWire();
    void processAudio();
    void checkSilence();

    // PipeWire structures
    struct pw_thread_loop* pw_loop_;
    struct pw_stream* pw_stream_;
    struct pw_properties* props_;
    struct spa_audio_info_raw spa_audio_info_;
    struct pw_buffer* pw_buffer_;
    
    // PipeWire event handlers
    struct pw_stream_events stream_events_;
    struct spa_hook stream_listener_;

    // Stream properties
    std::string target_device_;
    std::string stream_name_;
    bool auto_connect_;
    
    // Audio buffer management
    std::mutex buffer_mutex_;
    std::vector<uint8_t> temp_buffer_;
    size_t buffer_offset_;
    
    // Timing and state
    bool first_;
    std::chrono::time_point<std::chrono::steady_clock> last_process_time_;
    boost::asio::steady_timer check_timer_;
    std::chrono::microseconds silence_;
    
    // Configuration
    bool send_silence_;
    std::chrono::milliseconds idle_threshold_;
    std::string lastException_;
    
    // Stream state
    enum pw_stream_state stream_state_;
    bool running_;
};

} // namespace streamreader
