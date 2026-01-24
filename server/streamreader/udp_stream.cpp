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

#include "udp_stream.hpp"
#include "common/aixlog.hpp"
#include "common/str_compat.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cstring>

using namespace std;

namespace streamreader
{

static constexpr auto kUriBufferMs = "buffer_ms";
static constexpr auto kUriIdleThreshold = "idle_threshold";


UdpStream::UdpStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri,
                     PcmStream::Source source)
    : PcmStream(pcmListener, ioc, server_settings, uri, source), state_timer_(strand_), sap_timer_(strand_)
{
    // Default buffer_ms to 50ms if not specified, similar to other streams
    if (uri_.query.find(kUriBufferMs) == uri_.query.end())
        uri_.query[kUriBufferMs] = "50";

    buffer_ms_ = static_cast<uint32_t>(std::max(cpt::stoi(uri_.getQuery(kUriBufferMs, "50")), 0));
    idle_threshold_ = std::chrono::milliseconds(std::max(cpt::stoi(uri_.getQuery(kUriIdleThreshold, "100")), 10));

    // Determine if RTP mode is enabled (default to true if not specified, or checks scheme/params)
    is_rtp_ = uri_.getQuery("mode") == "rtp";

    recv_buffer_.resize(65536); // Max UDP size
}


UdpStream::~UdpStream()
{
    UdpStream::stop();
}


void UdpStream::start()
{
    PcmStream::start();

    try
    {
        udp::resolver resolver(strand_.get_inner_executor());
        udp::endpoint endpoint = *resolver.resolve(udp::v4(), uri_.host, std::to_string(uri_.port.value_or(0))).begin();

        socket_ = std::make_unique<udp::socket>(strand_.get_inner_executor());
        socket_->open(endpoint.protocol());
        socket_->bind(endpoint);

        LOG(INFO, "UdpStream") << "Listening on " << endpoint << "\n";

        do_read(); // Start reading

        // Setup SAP announcement if requested
        if (is_rtp_ || uri_.getQuery("sap") == "true")
        {
            sap_socket_ = std::make_unique<udp::socket>(strand_.get_inner_executor(), udp::endpoint(udp::v4(), 0));
            sap_endpoint_ = *resolver.resolve(udp::v4(), "224.2.127.254", "9875").begin();
            send_sap_announcement();
        }
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, "UdpStream") << "Failed to start UDP stream: " << e.what() << "\n";
        setState(ReaderState::kDisabled);
    }
}


void UdpStream::stop()
{
    if (socket_)
    {
        socket_->close();
        socket_.reset();
    }
    if (sap_socket_)
    {
        sap_socket_->close();
        sap_socket_.reset();
    }
    state_timer_.cancel();
    sap_timer_.cancel();
    PcmStream::stop();
}


void UdpStream::do_read()
{
    if (!socket_)
        return;

    // Extend idle check
    check_state(idle_threshold_ + std::chrono::milliseconds(chunk_ms_));

    socket_->async_receive_from(boost::asio::buffer(recv_buffer_), remote_endpoint_,
                                boost::asio::bind_executor(strand_,
                                                           [this, self = shared_from_this()](boost::system::error_code ec, std::size_t bytes_transferred)
    { handle_receive(ec, bytes_transferred); }));
}


void UdpStream::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
{
    state_timer_.cancel();

    if (error)
    {
        if (error == boost::asio::error::operation_aborted)
            return;
        LOG(ERROR, "UdpStream") << "Receive error: " << error.message() << "\n";
        do_read();
        return;
    }

    if (is_rtp_)
    {
        if (bytes_transferred >= 12)
        {
            RtpHeader header = parse_rtp_header(recv_buffer_.data(), bytes_transferred);
            process_rtp_packet(header, recv_buffer_.data(), bytes_transferred);
        }
    }
    else
    {
        // Raw UDP handling
        int frame_count = static_cast<int>(bytes_transferred / sampleFormat_.frameSize());
        size_t effective_len = frame_count * sampleFormat_.frameSize();

        if (frame_count > 0)
        {
            msg::PcmChunk packetChunk(sampleFormat_, 0);
            packetChunk.setFrameCount(frame_count);
            std::memcpy(packetChunk.payload, recv_buffer_.data(), effective_len);

            if (isSilent(packetChunk))
            {
                silence_ += packetChunk.duration<std::chrono::microseconds>();
                if (silence_ >= idle_threshold_)
                    setState(ReaderState::kIdle);
            }
            else
            {
                silence_ = std::chrono::microseconds(0);
                setState(ReaderState::kPlaying);
            }

            chunkRead(packetChunk);
        }
    }

    do_read();
}

UdpStream::RtpHeader UdpStream::parse_rtp_header(const char* data, size_t len)
{
    RtpHeader header{};
    if (len < 12)
        return header; // Caller ensures length

    uint8_t b0 = static_cast<uint8_t>(data[0]);
    uint8_t b1 = static_cast<uint8_t>(data[1]);

    header.version = (b0 >> 6) & 0x03;
    header.padding = (b0 >> 5) & 0x01;
    header.extension = (b0 >> 4) & 0x01;
    header.csrcCount = b0 & 0x0F;

    header.marker = (b1 >> 7) & 0x01;
    header.payloadType = b1 & 0x7F;

    uint16_t seq;
    std::memcpy(&seq, data + 2, 2);
    header.sequenceNumber = ntohs(seq);

    uint32_t ts;
    std::memcpy(&ts, data + 4, 4);
    header.timestamp = ntohl(ts);

    uint32_t ssrc;
    std::memcpy(&ssrc, data + 8, 4);
    header.ssrc = ntohl(ssrc);

    return header;
}

void UdpStream::process_rtp_packet(const RtpHeader& header, const char* data, size_t len)
{
    size_t header_len = 12 + static_cast<size_t>(header.csrcCount * 4);
    if (len < header_len)
        return;

    size_t payload_len = len - header_len;
    int frame_count = static_cast<int>(payload_len / sampleFormat_.frameSize());

    if (frame_count <= 0)
        return;

    msg::PcmChunk chunk(sampleFormat_, 0);
    chunk.setFrameCount(frame_count);
    std::memcpy(chunk.payload, data + header_len, frame_count * sampleFormat_.frameSize());

    if (first_packet_)
    {
        next_sequence_number_ = header.sequenceNumber;
        first_packet_ = false;
    }

    // Insert into jitter buffer
    jitter_buffer_[header.sequenceNumber] = std::move(chunk);

    pop_from_buffer();
}

void UdpStream::pop_from_buffer()
{
    // Max buffer depth (heuristic). Assuming ~20ms packets, 50ms buffer -> ~3 packets.
    // We add a safety margin because latency is better than skips.
    size_t max_packets = (buffer_ms_ / 20) + 10;

    // Loop to process consecutive packets
    int loops = 0;
    while (loops++ < 1000) // Safety break
    {
        if (jitter_buffer_.empty())
            break;

        auto it = jitter_buffer_.find(next_sequence_number_);
        if (it != jitter_buffer_.end())
        {
            // Found expected packet
            msg::PcmChunk& chunk = it->second;

            if (isSilent(chunk))
            {
                silence_ += chunk.duration<std::chrono::microseconds>();
                if (silence_ >= idle_threshold_)
                    setState(ReaderState::kIdle);
            }
            else
            {
                silence_ = std::chrono::microseconds(0);
                setState(ReaderState::kPlaying);
            }

            chunkRead(chunk);
            jitter_buffer_.erase(it);
            next_sequence_number_++;
        }
        else
        {
            // Check if buffer is too full (latency check)
            if (jitter_buffer_.size() > max_packets)
            {
                // Drop missing packet, advance expectation
                next_sequence_number_++;
            }
            else
            {
                // Wait for packet
                break;
            }
        }
    }
}


void UdpStream::check_state(const std::chrono::steady_clock::duration& duration)
{
    state_timer_.expires_after(duration);
    state_timer_.async_wait([this, self = shared_from_this()](const boost::system::error_code& ec)
    {
        if (!ec)
        {
            setState(ReaderState::kIdle);
        }
    });
}


void UdpStream::send_sap_announcement()
{
    if (!sap_socket_)
        return;

    // SAP Header: V=1, A=0, R=0, T=0, E=0, C=0 -> 0x20

    // SDP Payload
    std::string sdp = "v=0\r\n";
    sdp += "o=- " + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + " 0 IN IP4 " + uri_.host + "\r\n";
    sdp += "s=" + name_ + "\r\n";
    sdp += "c=IN IP4 " + uri_.host + "\r\n";
    sdp += "t=0 0\r\n";
    sdp += "m=audio " + std::to_string(uri_.port.value_or(0)) + " RTP/AVP " + (is_rtp_ ? "96" : "10") + "\r\n";

    if (is_rtp_)
        sdp += "a=rtpmap:96 L16/" + std::to_string(sampleFormat_.rate()) + "/" + std::to_string(sampleFormat_.channels()) + "\r\n";


    // SAP Packet Construction
    std::vector<uint8_t> packet;
    packet.push_back(0x20); // V=1, IPv4
    packet.push_back(0x00); // No Auth
    packet.push_back(0x12); // MsgId Hash
    packet.push_back(0x34);

    // IP Origin (4 bytes) - 0.0.0.0
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    // MIME type string "application/sdp\0"
    std::string mime = "application/sdp";
    packet.insert(packet.end(), mime.begin(), mime.end());
    packet.push_back(0);

    // SDP
    packet.insert(packet.end(), sdp.begin(), sdp.end());

    sap_socket_->async_send_to(boost::asio::buffer(packet), sap_endpoint_, [this, self = shared_from_this()](const boost::system::error_code&, std::size_t)
    {
        // Re-schedule
        sap_timer_.expires_after(std::chrono::seconds(10));
        sap_timer_.async_wait([this, self](const boost::system::error_code& ec)
        {
            if (!ec)
                send_sap_announcement();
        });
    });
}

} // namespace streamreader
