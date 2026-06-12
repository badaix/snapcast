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
#include "common/message/pcm_chunk.hpp"
#include "pcm_stream.hpp"

// 3rd party headers
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace streamreader
{

using boost::asio::ip::udp;

/// Reads and decodes PCM data from a UDP socket
/**
 * Reads PCM from a UDP socket and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmStream::Listener
 */
class UdpStream : public PcmStream
{
public:
    /// c'tor. Encoded PCM data is passed to the Listener
    UdpStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri,
              PcmStream::Source source);
    ~UdpStream() override;

    void start() override;
    void stop() override;

protected:
    /// Asynchronous read operation
    void do_read();

    /// Handle received data
    /// @param error Error code
    /// @param bytes_transferred Number of bytes received
    void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);

    /// Check stream state (playing/idle)
    /// @param duration Time to wait before next check
    void check_state(const std::chrono::steady_clock::duration& duration);

    /// RTP Header structure (RFC 3550)
    struct RtpHeader
    {
        uint8_t version{0};         ///< Protocol version
        bool padding{false};        ///< Padding flag
        bool extension{false};      ///< Extension flag
        uint8_t csrcCount{0};       ///< CSRC count
        bool marker{false};         ///< Marker bit
        uint8_t payloadType{0};     ///< Payload type
        uint16_t sequenceNumber{0}; ///< Sequence number
        uint32_t timestamp{0};      ///< Timestamp
        uint32_t ssrc{0};           ///< Synchronization source identifier
        size_t header_size{0};      ///< Total size of the parsed header
    };

    std::unique_ptr<udp::socket> socket_;      ///< UDP socket
    udp::endpoint remote_endpoint_;            ///< Sender endpoint
    std::vector<char> recv_buffer_;            ///< Receive buffer
    boost::asio::steady_timer state_timer_;    ///< Timer for state checking
    boost::asio::steady_timer sap_timer_;      ///< Timer for SAP announcements
    std::chrono::microseconds silence_{0};     ///< Accumulated silence duration
    std::chrono::milliseconds idle_threshold_; ///< Threshold to switch to idle state
    bool is_rtp_{false};                       ///< RTP mode flag

    // Ring Buffer for Jitter
    std::vector<msg::PcmChunk> ring_buffer_; ///< Fixed size ring buffer
    uint16_t playout_seq_{0};                ///< Next sequence number to play out
    bool buffering_{true};                   ///< Initial buffering state
    uint32_t buffer_ms_{50};                 ///< Jitter buffer latency config
    size_t ring_buffer_size_{0};             ///< Size of the ring buffer slots

    /// Processes a parsed RTP packet and adds it to the jitter buffer
    /// @param header Parsed RTP header
    /// @param payload Pointer to the RTP payload
    /// @param len Length of the payload
    void process_rtp_packet(const RtpHeader& header, const char* payload, size_t len);

    /// Pops available packets from the jitter buffer and sends them to the encoder
    void pop_from_buffer();

    /// Parses the RTP header from the received data using safe bound checks
    /// @param data Pointer to the packet data
    /// @param len Length of the packet data
    /// @return Parsed RTP header
    RtpHeader parse_rtp_header(const char* data, size_t len);

    // SAP announcements
    /// Sends a SAP announcement
    void send_sap_announcement();
    std::unique_ptr<udp::socket> sap_socket_; ///< Socket for SAP announcements
    udp::endpoint sap_endpoint_;              ///< SAP multicast endpoint
};

} // namespace streamreader
