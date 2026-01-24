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
#include "asio_stream.hpp"

// 3rd party headers
#include <boost/asio/ip/udp.hpp>

using boost::asio::ip::udp;


namespace streamreader
{

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
    void do_read();
    void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
    void check_state(const std::chrono::steady_clock::duration& duration);
    void on_timer(const boost::system::error_code& ec);

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
    };

    std::unique_ptr<udp::socket> socket_;
    udp::endpoint remote_endpoint_;
    std::vector<char> recv_buffer_;
    boost::asio::steady_timer state_timer_;
    boost::asio::steady_timer sap_timer_;
    std::chrono::microseconds silence_{0};
    std::chrono::milliseconds idle_threshold_;
    bool is_rtp_{false};

    // Jitter buffer
    std::map<uint16_t, msg::PcmChunk> jitter_buffer_;
    uint16_t next_sequence_number_{0};
    bool first_packet_{true};
    uint32_t buffer_ms_{50}; // Jitter buffer latency config

    /// Processes a parsed RTP packet and adds it to the jitter buffer
    void process_rtp_packet(const RtpHeader& header, const char* payload, size_t len);

    /// Pops available packets from the jitter buffer and sends them to the encoder
    void pop_from_buffer();

    /// Parses the RTP header from the received data
    RtpHeader parse_rtp_header(const char* data, size_t len);

    // SAP announcements
    void send_sap_announcement();
    std::unique_ptr<udp::socket> sap_socket_;
    udp::endpoint sap_endpoint_;
};

} // namespace streamreader
