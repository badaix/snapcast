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
#include "stream_session_ws.hpp"

// local headers
#include "common/aixlog.hpp"

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;

static constexpr auto LOG_TAG = "StreamSessionWS";


StreamSessionWebsocket::StreamSessionWebsocket(StreamMessageReceiver* receiver, const ServerSettings& server_settings, ssl_websocket&& ssl_ws)
    : StreamSession(ssl_ws.get_executor(), server_settings, receiver), ssl_ws_(std::move(ssl_ws)), is_ssl_(true)
{
    LOG(DEBUG, LOG_TAG) << "StreamSessionWS, mode: ssl\n";
}

StreamSessionWebsocket::StreamSessionWebsocket(StreamMessageReceiver* receiver, const ServerSettings& server_settings, tcp_websocket&& tcp_ws)
    : StreamSession(tcp_ws.get_executor(), server_settings, receiver), tcp_ws_(std::move(tcp_ws)), is_ssl_(false)
{
    LOG(DEBUG, LOG_TAG) << "StreamSessionWS, mode: tcp\n";
}


StreamSessionWebsocket::~StreamSessionWebsocket()
{
    LOG(DEBUG, LOG_TAG) << "~StreamSessionWS\n";
    stop(); // NOLINT
}


void StreamSessionWebsocket::start()
{
    // Read a message
    LOG(DEBUG, LOG_TAG) << "start\n";
    if (is_ssl_)
        ssl_ws_->binary(true);
    else
        tcp_ws_->binary(true);
    do_read_ws();
}


void StreamSessionWebsocket::stop()
{
    LOG(DEBUG, LOG_TAG) << "stop\n";
    boost::beast::error_code ec;
    if (is_ssl_)
    {
#if 0 // this might not return
        if (ssl_ws_->is_open())
            ssl_ws_->close(beast::websocket::close_code::normal, ec);
#endif

        if (ssl_ws_->next_layer().lowest_layer().is_open())
        {
            ssl_ws_->next_layer().lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            ssl_ws_->next_layer().lowest_layer().close(ec);
        }
    }
    else
    {
#if 0 // this might not return
        if (tcp_ws_->is_open())
            tcp_ws_->close(beast::websocket::close_code::normal, ec);
#endif

        if (tcp_ws_->next_layer().is_open())
        {
            tcp_ws_->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            tcp_ws_->next_layer().close(ec);
        }
    }
    if (ec)
        LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
    LOG(DEBUG, LOG_TAG) << "stopped\n";
}


std::string StreamSessionWebsocket::getIP()
{
    try
    {
        if (is_ssl_)
            return ssl_ws_->next_layer().lowest_layer().remote_endpoint().address().to_string();
        else
            return tcp_ws_->next_layer().lowest_layer().remote_endpoint().address().to_string();
    }
    catch (...)
    {
        return "0.0.0.0";
    }
}


void StreamSessionWebsocket::sendAsync(const shared_const_buffer& buffer, WriteHandler&& handler)
{
    LOG(TRACE, LOG_TAG) << "sendAsync: " << buffer.message().type << "\n";
    if (is_ssl_)
        ssl_ws_->async_write(buffer, [self = shared_from_this(), buffer, handler = std::move(handler)](boost::system::error_code ec, std::size_t length)
        {
            if (handler)
                handler(ec, length);
        });
    else
        tcp_ws_->async_write(buffer, [self = shared_from_this(), buffer, handler = std::move(handler)](boost::system::error_code ec, std::size_t length)
        {
            if (handler)
                handler(ec, length);
        });
}


void StreamSessionWebsocket::do_read_ws()
{
    // Read a message into our buffer
    if (is_ssl_)
        ssl_ws_->async_read(buffer_,
                            [this, self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) { on_read_ws(ec, bytes_transferred); });
    else
        tcp_ws_->async_read(buffer_,
                            [this, self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) { on_read_ws(ec, bytes_transferred); });
}


void StreamSessionWebsocket::on_read_ws(beast::error_code ec, std::size_t bytes_transferred)
{
    tv now;
    LOG(DEBUG, LOG_TAG) << "on_read_ws, ec: " << ec << ", bytes_transferred: " << bytes_transferred << "\n";

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

    auto* data = static_cast<char*>(buffer_.data().data());
    baseMessage_.deserialize(data);
    LOG(DEBUG, LOG_TAG) << "getNextMessage: " << baseMessage_.type << ", size: " << baseMessage_.size << ", id: " << baseMessage_.id
                        << ", refers: " << baseMessage_.refersTo << "\n";

    baseMessage_.received = now;
    if (messageReceiver_ != nullptr)
        messageReceiver_->onMessageReceived(shared_from_this(), baseMessage_, data + base_msg_size_);

    buffer_.consume(bytes_transferred);
    do_read_ws();
}
