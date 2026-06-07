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
#include "tcp_stream.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"

// 3rd party headers

// standard headers
#include <cstdint>
#include <memory>
#include <string>


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "TcpStream";

namespace
{

bool getBoolQuery(const StreamUri& uri, const std::string& key, bool fallback = false)
{
    auto value = utils::string::tolower_copy(uri.getQuery(key, fallback ? "true" : "false"));
    return (value == "1") || (value == "true") || (value == "yes") || (value == "on");
}

void unpackS24LeToS24_32Le(const char* src, char* dst, size_t sample_count)
{
    for (size_t idx = 0; idx < sample_count; ++idx)
    {
        auto src_idx = idx * 3;
        auto dst_idx = idx * 4;
        dst[dst_idx] = src[src_idx];
        dst[dst_idx + 1] = src[src_idx + 1];
        dst[dst_idx + 2] = src[src_idx + 2];
        dst[dst_idx + 3] = (static_cast<unsigned char>(src[src_idx + 2]) & 0x80U) != 0U ? static_cast<char>(0xFF) : 0;
    }
}

} // namespace

TcpStream::TcpStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri,
                     PcmStream::Source source)
    : AsioStream<tcp::socket>(pcmListener, ioc, server_settings, uri, source), packed_s24le_(false), packed_payload_size_(0), reconnect_timer_(ioc)
{
    static constexpr uint16_t DEFAULT_PORT = 4953;
    host_ = uri_.host;
    port_ = uri_.port.value_or(DEFAULT_PORT);

    auto mode = uri_.getQuery("mode", "server");
    if (mode == "server")
        is_server_ = true;
    else if (mode == "client")
        is_server_ = false;
    else
        throw SnapException("mode must be 'client' or 'server'");

    port_ = cpt::stoi(uri_.getQuery("port", cpt::to_string(port_)), port_);

    // Allow ffmpeg-compatible packed 24-bit little endian input on TCP streams.
    // Snapcast internally uses 24-bit padded to 32-bit words, so convert on ingest.
    packed_s24le_ = getBoolQuery(uri_, "packed_s24le", false) || getBoolQuery(uri_, "packed_s24", false);
    if (packed_s24le_)
    {
        if (sampleFormat_.bits() != 24)
        {
            LOG(WARNING, LOG_TAG) << "packed_s24le=true only applies to 24-bit streams, got sample format: " << sampleFormat_.toString() << "\n";
            packed_s24le_ = false;
        }
        else
        {
            packed_payload_size_ = chunk_->getSampleCount() * 3;
            packed_read_buffer_.resize(packed_payload_size_);
            LOG(INFO, LOG_TAG) << "Enabled packed_s24le ingest for stream '" << getName() << "', read size: " << packed_payload_size_
                               << ", chunk size: " << chunk_->payloadSize << "\n";
        }
    }

    LOG(INFO, LOG_TAG) << "TcpStream host: " << host_ << ", port: " << port_ << ", is server: " << is_server_ << "\n";
    if (is_server_)
        acceptor_ = make_unique<tcp::acceptor>(strand_, tcp::endpoint(boost::asio::ip::make_address(host_), port_));
}


void TcpStream::connect()
{
    if (!active_)
        return;

    if (is_server_)
    {
        acceptor_->async_accept([this, self = shared_from_this()](boost::system::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                LOG(DEBUG, LOG_TAG) << "New client connection\n";
                stream_ = make_unique<tcp::socket>(std::move(socket));
                on_connect();
            }
            else
            {
                LOG(ERROR, LOG_TAG) << "Accept failed: " << ec.message() << "\n";
            }
        });
    }
    else
    {
        stream_ = make_unique<tcp::socket>(strand_);
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(host_), port_);
        stream_->async_connect(endpoint, [this, self = shared_from_this()](const boost::system::error_code& ec)
        {
            if (!ec)
            {
                LOG(DEBUG, LOG_TAG) << "Connected\n";
                on_connect();
            }
            else
            {
                LOG(DEBUG, LOG_TAG) << "Connect failed: " << ec.message() << "\n";
                wait(reconnect_timer_, 1s, [this, self = shared_from_this()] { connect(); });
            }
        });
    }
}


void TcpStream::disconnect()
{
    reconnect_timer_.cancel();
    if (acceptor_)
        acceptor_->cancel();
    AsioStream<tcp::socket>::disconnect();
}

void TcpStream::do_read()
{
    if (!packed_s24le_)
    {
        AsioStream<tcp::socket>::do_read();
        return;
    }

    // Reset the silence timer
    check_state(idle_threshold_ + std::chrono::milliseconds(chunk_ms_));
    boost::asio::async_read(*stream_, boost::asio::buffer(packed_read_buffer_.data(), packed_payload_size_),
                            [this, self = shared_from_this()](boost::system::error_code ec, std::size_t length) mutable
    {
        state_timer_.cancel();

        if (ec)
        {
            if (lastException_ != ec.message())
            {
                LOG(ERROR, LOG_TAG) << "Error reading message in stream '" << getName() << "': " << ec.message() << ", length: " << length
                                    << ", ec: " << ec << "\n";
                lastException_ = ec.message();
            }
            disconnect();
            wait(read_timer_, 100ms, [this, self = shared_from_this()] { connect(); });
            return;
        }

        lastException_.clear();
        unpackS24LeToS24_32Le(packed_read_buffer_.data(), chunk_->payload, chunk_->getSampleCount());

        if (isSilent(*chunk_))
        {
            silence_ += chunk_->duration<std::chrono::microseconds>();
            if (silence_ >= idle_threshold_)
            {
                setState(ReaderState::kIdle);
                // Avoid overflow
                silence_ = idle_threshold_;
            }
        }
        else
        {
            silence_ = 0ms;
            setState(ReaderState::kPlaying);
        }

        if (first_)
        {
            first_ = false;
            tvEncodedChunk_ = std::chrono::steady_clock::now() - chunk_->duration<std::chrono::nanoseconds>();
            nextTick_ = std::chrono::steady_clock::now();
        }

        chunkRead(*chunk_);
        nextTick_ += chunk_->duration<std::chrono::nanoseconds>();
        auto currentTick = std::chrono::steady_clock::now();

        // Synchronize read to chunk_ms_
        if (nextTick_ >= currentTick)
        {
            read_timer_.expires_after(nextTick_ - currentTick);
            read_timer_.async_wait([this, self = shared_from_this()](const boost::system::error_code& timer_ec)
            {
                if (timer_ec)
                {
                    LOG(ERROR, LOG_TAG) << "Error during async wait in stream '" << getName() << "': " << timer_ec.message() << "\n";
                }
                else
                {
                    do_read();
                }
            });
            return;
        }
        // Read took longer, wait for the buffer to fill up
        else
        {
            resync(std::chrono::duration_cast<std::chrono::nanoseconds>(currentTick - nextTick_));
            first_ = true;
            do_read();
        }
    });
}


} // namespace streamreader
