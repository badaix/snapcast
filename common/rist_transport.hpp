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

#pragma once

#ifdef HAS_LIBRIST

// local headers
#include "aixlog.hpp"
#include "message/message.hpp"
#include "message/hello.hpp"
#include "message/server_settings.hpp"  
#include "message/codec_header.hpp"
#include "message/pcm_chunk.hpp"

// 3rd party headers
#include <librist/librist.h>

// standard headers
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

// Optimized parameters for low-latency RIST in [ms]
static constexpr auto recovery_length_min = 20; // was 200 before
static constexpr auto recovery_length_max = 50; // was 200 before
static constexpr auto recovery_rtt_min = 5; // was 5 before
static constexpr auto recovery_rtt_max = 50; // was 500 before
static constexpr auto recovery_reorder_buffer = 15; // was 15 before
static constexpr auto min_retries = 3; // was 6 before
static constexpr auto max_retries = 10; // was 20 before
static constexpr auto congestion_control_mode = RIST_CONGESTION_CONTROL_MODE_NORMAL;

// RIST logging callback
static constexpr auto LOG_LIBRIST_TAG = "libRIST";

static constexpr auto LOG_GLOBAL = " (global) ";
static constexpr auto LOG_TRANSPORT = " (transport) ";
static constexpr auto LOG_RECEIVER = " (receiver) ";
static constexpr auto LOG_SENDER = " (sender) ";

// TODO: Does this causes crashes in libRIST?
int rist_log_callback(void* arg, enum rist_log_level level, const char* msg);

/// Forward declarations
namespace msg {
    struct BaseMessage;
    class PcmChunk;
}

/// RIST transport callback interface
class RistTransportReceiver
{
public:
    virtual ~RistTransportReceiver() = default;
    
    /// Called when a message is received via RIST
    /// For zero-copy optimization: if payload is empty, use payload_ptr + payload_size instead
    virtual void onRistMessageReceived(const msg::BaseMessage& baseMessage, const std::string& payload, 
                                      const char* payload_ptr /* , size_t payload_size, uint16_t vport */) = 0;
    /// Called when a client connects (server side only)
    virtual void onRistClientConnected(const std::string& clientId) = 0;
    /// Called when a client disconnects (server side only)  
    virtual void onRistClientDisconnected(const std::string& clientId) = 0;
};

/// Bidirectional RIST transport using virtual port multiplexing (testrist model)
/// Can be used by both server and client
class RistTransport
{
public:
    /// Virtual ports following testrist model
    static constexpr uint16_t VPORT_AUDIO = 1000;
    static constexpr uint16_t VPORT_CONTROL = 2000; 
    static constexpr uint16_t VPORT_BACKCHANNEL = 3000;

    /// Transport mode
    enum class Mode {
        SERVER,  ///< Server mode (bind and accept connections)
        CLIENT   ///< Client mode (connect to server)
    };

    /// c'tor
    RistTransport(Mode mode, RistTransportReceiver* receiver = nullptr);
    /// d'tor
    virtual ~RistTransport();

    /// Configure as server (bind to address/port)
    bool configureServer(const std::string& bind_address, uint16_t port);
    /// Configure as client (connect to server address/port)  
    bool configureClient(const std::string& server_address, uint16_t port);
    /// Configure as client with custom RIST parameters
    bool configureClient(const std::string& server_address, uint16_t port, uint32_t recovery_length_min, uint32_t recovery_length_max);

    /// Start RIST transport
    bool start();
    /// Stop RIST transport
    void stop();

    /// Send message on specified virtual port
    bool sendMessage(uint16_t vport, const msg::BaseMessage& message);
    /// Send raw data directly on specified virtual port (for pre-serialized data)
    bool sendRawData(uint16_t vport, const void* data, size_t size);
    /// Send audio chunk (convenience method for VPORT_AUDIO)
    bool sendAudioChunk(const std::shared_ptr<msg::PcmChunk>& chunk);
    
    /// Update RIST parameters and restart transport (client mode only)
    bool updateClientParameters(uint32_t recovery_length_min, uint32_t recovery_length_max);

private:
    /// RIST data callback (static for C API)
    static int dataCallback(void* arg, struct rist_data_block* data_block);
    /// Instance data callback handler
    int handleDataCallback(struct rist_data_block* data_block);
    /// Helper to create and configure RIST peer
    bool createPeer(struct rist_ctx* ctx, const std::string& url, const std::string& type);

    Mode mode_;
    RistTransportReceiver* receiver_;
    
    struct rist_ctx* sender_ctx_;
    struct rist_ctx* receiver_ctx_;
    
    std::string address_;
    uint16_t port_;
    
    /// Custom RIST parameters (0 = use defaults)
    uint32_t custom_recovery_length_min_;
    uint32_t custom_recovery_length_max_;
    
    /// Track connected clients by clientId (server mode only)
    std::unordered_map<std::string, bool> connected_clients_;
    
    bool running_;
    rist_logging_settings log_settings_;

public:
    static constexpr auto LOG_TAG = "RistTransport";
};

#endif // HAS_LIBRIST
