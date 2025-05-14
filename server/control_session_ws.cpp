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
#include "control_session_ws.hpp"

// local headers
#include "common/aixlog.hpp"

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;

static constexpr auto LOG_TAG = "ControlSessionWS";


ControlSessionWebsocket::ControlSessionWebsocket(ControlMessageReceiver* receiver, ssl_websocket&& ssl_ws, const ServerSettings& settings)
    : ControlSession(receiver, settings), ssl_ws_(std::move(ssl_ws)), strand_(boost::asio::make_strand(ssl_ws_->get_executor())), is_ssl_(true)
{
    LOG(DEBUG, LOG_TAG) << "ControlSessionWebsocket, mode: ssl\n";
}

ControlSessionWebsocket::ControlSessionWebsocket(ControlMessageReceiver* receiver, tcp_websocket&& tcp_ws, const ServerSettings& settings)
    : ControlSession(receiver, settings), tcp_ws_(std::move(tcp_ws)), strand_(boost::asio::make_strand(tcp_ws_->get_executor())), is_ssl_(false)
{
    LOG(DEBUG, LOG_TAG) << "ControlSessionWebsocket, mode: tcp\n";
}


ControlSessionWebsocket::~ControlSessionWebsocket()
{
    LOG(DEBUG, LOG_TAG) << "ControlSessionWebsocket::~ControlSessionWebsocket()\n";
    stop();
}


void ControlSessionWebsocket::start()
{
    // Read a message
    do_read_ws();
}


void ControlSessionWebsocket::stop()
{
    // if (ws_.is_open())
    // {
    //     boost::beast::error_code ec;
    //     ws_.close(beast::websocket::close_code::normal, ec);
    //     if (ec)
    //         LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
    // }
}


void ControlSessionWebsocket::sendAsync(const std::string& message)
{
    boost::asio::post(strand_, [this, self = shared_from_this(), msg = message]() mutable
    {
        messages_.push_back(std::move(msg));
        if (messages_.size() > 1)
        {
            LOG(DEBUG, LOG_TAG) << "HTTP session outstanding async_writes: " << messages_.size() << "\n";
            return;
        }
        send_next();
    });
}


void ControlSessionWebsocket::send_next()
{
    const std::string& message = messages_.front();

    auto write_handler = [this, self = shared_from_this()](std::error_code ec, std::size_t length)
    {
        messages_.pop_front();
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error while writing to web socket: " << ec.message() << "\n";
        }
        else
        {
            LOG(TRACE, LOG_TAG) << "Wrote " << length << " bytes to web socket\n";
        }
        if (!messages_.empty())
            send_next();
    };

    if (is_ssl_)
    {
        ssl_ws_->async_write(boost::asio::buffer(message), [write_handler](std::error_code ec, std::size_t length) { write_handler(ec, length); });
    }
    else
    {
        tcp_ws_->async_write(boost::asio::buffer(message), [write_handler](std::error_code ec, std::size_t length) { write_handler(ec, length); });
    }
}


void ControlSessionWebsocket::do_read_ws()
{
    // Read a message into our buffer
    if (is_ssl_)
        ssl_ws_->async_read(buffer_,
                            [this, self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) { on_read_ws(ec, bytes_transferred); });
    else
        tcp_ws_->async_read(buffer_,
                            [this, self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) { on_read_ws(ec, bytes_transferred); });
}


void ControlSessionWebsocket::on_read_ws(beast::error_code ec, std::size_t bytes_transferred)
{
    // This indicates that the session was closed
    if (ec == websocket::error::closed)
        return;

    if (ec)
    {
        LOG(ERROR, LOG_TAG) << "ControlSessionWebsocket::on_read_ws error: " << ec.message() << "\n";
        return;
    }

    std::string line{boost::beast::buffers_to_string(buffer_.data())};
    if (!line.empty())
    {
        // LOG(DEBUG, LOG_TAG) << "received: " << line << "\n";
        if (message_receiver_ != nullptr)
        {
            message_receiver_->onMessageReceived(shared_from_this(), line, [this](const std::string& response)
            {
                if (!response.empty())
                {
                    sendAsync(response);
                }
            });
        }
    }
    buffer_.consume(bytes_transferred);
    do_read_ws();
}
