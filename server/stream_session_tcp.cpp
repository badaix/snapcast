/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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
#include "stream_session_tcp.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/message/pcm_chunk.hpp"

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;
using namespace streamreader;


static constexpr auto LOG_TAG = "StreamSessionTCP";


StreamSessionTcp::StreamSessionTcp(StreamMessageReceiver* receiver, tcp::socket&& socket)
    : StreamSession(socket.get_executor(), receiver), socket_(std::move(socket))
{
}


StreamSessionTcp::~StreamSessionTcp()
{
    LOG(DEBUG, LOG_TAG) << "~StreamSessionTcp\n";
    stop();
}


void StreamSessionTcp::start()
{
    read_next();
}


void StreamSessionTcp::stop()
{
    LOG(DEBUG, LOG_TAG) << "stop\n";
    if (socket_.is_open())
    {
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
            LOG(ERROR, LOG_TAG) << "Error in socket shutdown: " << ec.message() << "\n";
        socket_.close(ec);
        if (ec)
            LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
        LOG(DEBUG, LOG_TAG) << "stopped\n";
    }
}


std::string StreamSessionTcp::getIP()
{
    try
    {
        return socket_.remote_endpoint().address().to_string();
    }
    catch (...)
    {
        return "0.0.0.0";
    }
}


void StreamSessionTcp::read_next()
{
    boost::asio::async_read(socket_, boost::asio::buffer(buffer_, base_msg_size_),
                            [this, self = shared_from_this()](boost::system::error_code ec, std::size_t length) mutable
                            {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error reading message header of length " << length << ": " << ec.message() << "\n";
            messageReceiver_->onDisconnect(this);
            return;
        }

        baseMessage_.deserialize(buffer_.data());
        LOG(DEBUG, LOG_TAG) << "getNextMessage: " << baseMessage_.type << ", size: " << baseMessage_.size << ", id: " << baseMessage_.id
                            << ", refers: " << baseMessage_.refersTo << "\n";
        if (baseMessage_.type > message_type::kLast)
        {
            LOG(ERROR, LOG_TAG) << "unknown message type received: " << baseMessage_.type << ", size: " << baseMessage_.size << "\n";
            messageReceiver_->onDisconnect(this);
            return;
        }
        else if (baseMessage_.size > msg::max_size)
        {
            LOG(ERROR, LOG_TAG) << "received message of type " << baseMessage_.type << " to large: " << baseMessage_.size << "\n";
            messageReceiver_->onDisconnect(this);
            return;
        }

        if (baseMessage_.size > buffer_.size())
            buffer_.resize(baseMessage_.size);

        boost::asio::async_read(socket_, boost::asio::buffer(buffer_, baseMessage_.size),
                                [this, self](boost::system::error_code ec, std::size_t length) mutable
                                {
            if (ec)
            {
                LOG(ERROR, LOG_TAG) << "Error reading message body of length " << length << ": " << ec.message() << "\n";
                messageReceiver_->onDisconnect(this);
                return;
            }

            tv t;
            baseMessage_.received = t;
            if (messageReceiver_ != nullptr)
                messageReceiver_->onMessageReceived(this, baseMessage_, buffer_.data());
            read_next();
        });
    });
}


void StreamSessionTcp::sendAsync(const shared_const_buffer& buffer, const WriteHandler& handler)
{
    boost::asio::async_write(socket_, buffer,
                             [self = shared_from_this(), buffer, handler](boost::system::error_code ec, std::size_t length) { handler(ec, length); });
}
