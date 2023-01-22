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
#include "stream_session_ws.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/message/pcm_chunk.hpp"

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;

static constexpr auto LOG_TAG = "StreamSessionWS";


StreamSessionWebsocket::StreamSessionWebsocket(StreamMessageReceiver* receiver, websocket::stream<beast::tcp_stream>&& socket)
    : StreamSession(socket.get_executor(), receiver), ws_(std::move(socket))
{
    LOG(DEBUG, LOG_TAG) << "StreamSessionWS\n";
}


StreamSessionWebsocket::~StreamSessionWebsocket()
{
    LOG(DEBUG, LOG_TAG) << "~StreamSessionWS\n";
    stop();
}


void StreamSessionWebsocket::start()
{
    // Read a message
    LOG(DEBUG, LOG_TAG) << "start\n";
    ws_.binary(true);
    do_read_ws();
}


void StreamSessionWebsocket::stop()
{
    if (ws_.is_open())
    {
        boost::beast::error_code ec;
        ws_.close(beast::websocket::close_code::normal, ec);
        if (ec)
            LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
    }
}


std::string StreamSessionWebsocket::getIP()
{
    try
    {
        return ws_.next_layer().socket().remote_endpoint().address().to_string();
    }
    catch (...)
    {
        return "0.0.0.0";
    }
}


void StreamSessionWebsocket::sendAsync(const shared_const_buffer& buffer, const WriteHandler& handler)
{
    LOG(TRACE, LOG_TAG) << "sendAsync: " << buffer.message().type << "\n";
    ws_.async_write(buffer, [self = shared_from_this(), buffer, handler](boost::system::error_code ec, std::size_t length) { handler(ec, length); });
}


void StreamSessionWebsocket::do_read_ws()
{
    // Read a message into our buffer
    ws_.async_read(buffer_, [this, self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) { on_read_ws(ec, bytes_transferred); });
}


void StreamSessionWebsocket::on_read_ws(beast::error_code ec, std::size_t bytes_transferred)
{
    LOG(DEBUG, LOG_TAG) << "on_read_ws, ec: " << ec << ", bytes_transferred: " << bytes_transferred << "\n";
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed)
    {
        messageReceiver_->onDisconnect(this);
        return;
    }

    if (ec)
    {
        LOG(ERROR, LOG_TAG) << "ControlSessionWebsocket::on_read_ws error: " << ec.message() << "\n";
        messageReceiver_->onDisconnect(this);
        return;
    }

    auto* data = boost::asio::buffer_cast<char*>(buffer_.data());
    baseMessage_.deserialize(data);
    LOG(DEBUG, LOG_TAG) << "getNextMessage: " << baseMessage_.type << ", size: " << baseMessage_.size << ", id: " << baseMessage_.id
                        << ", refers: " << baseMessage_.refersTo << "\n";

    tv t;
    baseMessage_.received = t;
    if (messageReceiver_ != nullptr)
        messageReceiver_->onMessageReceived(this, baseMessage_, data + base_msg_size_);

    buffer_.consume(bytes_transferred);
    do_read_ws();
}
