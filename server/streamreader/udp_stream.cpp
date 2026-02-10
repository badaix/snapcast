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
#include <limits>

using namespace std;

namespace streamreader
{

static constexpr auto kUriBufferMs = "buffer_ms";
static constexpr auto kUriIdleThreshold = "idle_threshold";
// WARNING: kDefaultRingBufferSize MUST be a power of 2 (e.g., 512, 1024) to ensure
// proper sequence number wrap-around mapping for uint16_t (65536 is a multiple of powers of 2).
static constexpr size_t kDefaultRingBufferSize = 1024; // Slots for ~20s of 20ms packets

UdpStream::UdpStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri,
                     PcmStream::Source source)
    : PcmStream(pcmListener, ioc, server_settings, uri, source), state_timer_(strand_), sap_timer_(strand_)
{
    // Default buffer_ms to 50ms if not specified
    if (uri_.query.find(kUriBufferMs) == uri_.query.end())
        uri_.query[kUriBufferMs] = "50";

    buffer_ms_ = static_cast<uint32_t>(std::max(cpt::stoi(uri_.getQuery(kUriBufferMs, "50")), 0));
    idle_threshold_ = std::chrono::milliseconds(std::max(cpt::stoi(uri_.getQuery(kUriIdleThreshold, "100")), 10));

    is_rtp_ = uri_.getQuery("mode") == "rtp";

    recv_buffer_.resize(65536); // Max UDP size

    // Initialize Ring Buffer
    ring_buffer_size_ = kDefaultRingBufferSize;
    ring_buffer_.resize(ring_buffer_size_, msg::PcmChunk(sampleFormat_, 0));
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
        RtpHeader header = parse_rtp_header(recv_buffer_.data(), bytes_transferred);
        if (header.header_size > 0)
        {
            process_rtp_packet(header, recv_buffer_.data(), bytes_transferred);
        }
        else
        {
            LOG(WARNING, "UdpStream") << "Invalid RTP header\n";
        }
    }
    else
    {
        // Raw UDP handling
        int frame_count = static_cast<int>(bytes_transferred / sampleFormat_.frameSize());
        size_t effective_len = frame_count * sampleFormat_.frameSize();

        if (frame_count > 0 && effective_len <= bytes_transferred)
        {
            msg::PcmChunk packetChunk(sampleFormat_, 0);
            packetChunk.setFrameCount(frame_count);
            // Safe copy
            std::copy(recv_buffer_.begin(), recv_buffer_.begin() + effective_len, packetChunk.payload);

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
    // RTP Header min size is 12 bytes
    if (len < 12)
        return header;

    // Safe access
    // Byte 0
    auto b0 = static_cast<uint8_t>(data[0]);
    header.version = (b0 >> 6) & 0x03;
    header.padding = (b0 >> 5) & 0x01;
    header.extension = (b0 >> 4) & 0x01;
    header.csrcCount = b0 & 0x0F;

    // Byte 1
    auto b1 = static_cast<uint8_t>(data[1]);
    header.marker = (b1 >> 7) & 0x01;
    header.payloadType = b1 & 0x7F;

    // Bytes 2,3: Sequence Number
    // Use safe copy to avoid unaligned access ub (though rare on modern x86, good practice)
    uint16_t seq_n;
    std::copy_n(data + 2, 2, reinterpret_cast<char*>(&seq_n));
    header.sequenceNumber = ntohs(seq_n);

    // Bytes 4-7: Timestamp
    uint32_t ts_n;
    std::copy_n(data + 4, 4, reinterpret_cast<char*>(&ts_n));
    header.timestamp = ntohl(ts_n);

    // Bytes 8-11: SSRC
    uint32_t ssrc_n;
    std::copy_n(data + 8, 4, reinterpret_cast<char*>(&ssrc_n));
    header.ssrc = ntohl(ssrc_n);

    size_t header_len = 12 + static_cast<size_t>(header.csrcCount * 4);
    if (len < header_len)
        return header; // Invalid length for CSRCs

    header.header_size = header_len;
    return header;
}

void UdpStream::process_rtp_packet(const RtpHeader& header, const char* data, size_t len)
{
    if (len < header.header_size)
        return;

    size_t payload_len = len - header.header_size;
    int frame_count = static_cast<int>(payload_len / sampleFormat_.frameSize());

    if (frame_count <= 0)
        return;

    // Initialize playout sequence on first packet
    if (buffering_ && playout_seq_ == 0 && header.sequenceNumber != 0)
    {
        playout_seq_ = header.sequenceNumber;
    }

    // Sequence wrap-around handling
    // We map sequence number to index: seq % size
    size_t index = header.sequenceNumber % ring_buffer_size_;

    // Safety: Ensure index is valid (modulo always is, but being explicit)
    if (index >= ring_buffer_.size())
        return;

    // Prepare chunk
    msg::PcmChunk& chunk = ring_buffer_[index];
    chunk.setFrameCount(frame_count);

    // Bounds check for payload copy
    size_t copy_len = frame_count * sampleFormat_.frameSize();
    if (header.header_size + copy_len > len)
    {
        LOG(WARNING, "UdpStream") << "Payload truncation risk, discarding\n";
        return;
    }

    std::copy_n(data + header.header_size, copy_len, chunk.payload);

    // Logic:
    // If we are buffering, check if we have enough
    // How to determine 'enough'? simple heuristic: difference between seq and playout_seq
    if (buffering_)
    {
        int packets_needed = buffer_ms_ / 20;
        if (packets_needed < 1)
            packets_needed = 1;

        // Calculate difference considering potential wrap-around for uint16_t
        // This cast handles positive and negative differences correctly for uint16_t
        auto diff = static_cast<int16_t>(header.sequenceNumber - playout_seq_);

        if (diff >= packets_needed)
        {
            buffering_ = false;
        }
    }

    pop_from_buffer();
}

void UdpStream::pop_from_buffer()
{
    if (buffering_)
        return;

    // Output any available consecutive packets
    size_t iterations = 0;
    while (iterations++ < ring_buffer_size_) // Safety break: process at most one full buffer
    {
        size_t index = playout_seq_ % ring_buffer_size_;
        msg::PcmChunk& chunk = ring_buffer_[index];

        if (chunk.getFrameCount() > 0)
        {
            // Found data for the current sequence
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

            // "Consume" the chunk (mark empty)
            chunk.setFrameCount(0);

            playout_seq_++;
        }
        else
        {
            // Gap handling (Packet Loss or Underrun)
            // Check if we have "future" packets to decide if we should skip this one
            bool future_packets_exist = false;
            // Look ahead a few packets (e.g., 4 packets ~80ms)
            for (uint16_t lookahead = 1; lookahead <= 4; ++lookahead)
            {
                // Handle uint16_t wrap-around for sequence number addition
                uint16_t next_seq = playout_seq_ + lookahead;
                size_t next_idx = next_seq % ring_buffer_size_;

                if (ring_buffer_[next_idx].getFrameCount() > 0)
                {
                    future_packets_exist = true;
                    break;
                }
            }

            if (future_packets_exist)
            {
                // We have future packets, so the current one is likely lost.
                // Log and skip it to prevent stalling the stream.
                // LOG(WARNING, "UdpStream") << "Packet loss detected at seq " << playout_seq_ << ", skipping.\n";

                // TODO: Insert PLC (Packet Loss Concealment) silence chunk here if desired

                playout_seq_++; // Skip the missing packet
            }
            else
            {
                // No future packets found either. This is likely a genuine buffer underrun.
                // Stop and wait for more data.
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
    std::string sdp = "v=0\r\n";
    sdp += "o=- " + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + " 0 IN IP4 " + uri_.host + "\r\n";
    sdp += "s=" + name_ + "\r\n";
    sdp += "c=IN IP4 " + uri_.host + "\r\n";
    sdp += "t=0 0\r\n";
    sdp += "m=audio " + std::to_string(uri_.port.value_or(0)) + " RTP/AVP " + (is_rtp_ ? "96" : "10") + "\r\n";

    if (is_rtp_)
        sdp += "a=rtpmap:96 L16/" + std::to_string(sampleFormat_.rate()) + "/" + std::to_string(sampleFormat_.channels()) + "\r\n";


    std::vector<uint8_t> packet;
    packet.push_back(0x20); // V=1, IPv4
    packet.push_back(0x00); // No Auth
    packet.push_back(0x12); // MsgId Hash
    packet.push_back(0x34);
    packet.push_back(0); // Origin 0.0.0.0
    packet.push_back(0);
    packet.push_back(0);
    packet.push_back(0);

    std::string mime = "application/sdp";
    packet.insert(packet.end(), mime.begin(), mime.end());
    packet.push_back(0);
    packet.insert(packet.end(), sdp.begin(), sdp.end());

    sap_socket_->async_send_to(boost::asio::buffer(packet), sap_endpoint_, [this, self = shared_from_this()](const boost::system::error_code&, std::size_t)
    {
        sap_timer_.expires_after(std::chrono::seconds(10));
        sap_timer_.async_wait([this, self](const boost::system::error_code& ec)
        {
            if (!ec)
                send_sap_announcement();
        });
    });
}

} // namespace streamreader
