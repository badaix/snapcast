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
#include <spa/param/audio/raw.h>
#include <boost/asio/io_context.hpp>

// standard headers
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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
    static void on_core_info(void* userdata, const struct pw_core_info* info);
    static void on_core_error(void* userdata, uint32_t id, int seq, int res, const char* message);
    
    void initPipeWire();
    void uninitPipeWire();
    void processAudio();

    // PipeWire structures
    struct pw_main_loop* pw_main_loop_;
    struct pw_context* pw_context_;
    struct pw_core* pw_core_;
    struct pw_stream* pw_stream_;
    struct pw_properties* props_;
    
    // PipeWire event handlers
    struct pw_core_events core_events_;
    struct spa_hook core_listener_;
    struct pw_stream_events stream_events_;
    struct spa_hook stream_listener_;

    // Stream properties
    std::string target_device_;
    std::string stream_name_;
    bool capture_sink_;
    
    // Audio buffer management
    std::mutex buffer_mutex_;
    std::vector<uint8_t> temp_buffer_;
    
    // Timing and state
    bool first_;
    std::chrono::microseconds silence_;
    
    // Configuration
    bool send_silence_;
    std::chrono::milliseconds idle_threshold_;
    
    // Stream state
    enum pw_stream_state stream_state_;
    bool running_;
    
    // PipeWire thread
    std::thread pw_thread_;
};

} // namespace streamreader
