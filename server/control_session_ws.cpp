/***
    This file is part of snapcast
    Copyright (C) 2014-2019  Johannes Pohl

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

#include "control_session_ws.hpp"
#include "aixlog.hpp"
#include "message/pcmChunk.h"
#include <iostream>

using namespace std;


ControlSessionWs::ControlSessionWs(ControlMessageReceiver* receiver, tcp::socket&& socket) : ControlSession(receiver), ws_(std::move(socket))
{
}


ControlSessionWs::~ControlSessionWs()
{
    LOG(DEBUG) << "ControlSessionWs::~ControlSessionWs()\n";
    stop();
}


void ControlSessionWs::start()
{
    // Set suggested timeout settings for the websocket
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) { res.set(http::field::server, "Snapcast websocket-server"); }));

    // Accept the websocket handshake
    auto self(shared_from_this());
    ws_.async_accept([this, self](beast::error_code ec) { on_accept(ec); });
}


void ControlSessionWs::on_accept(beast::error_code ec)
{
    if (ec)
    {
        LOG(ERROR) << "ControlSessionWs::on_accept, error: " << ec.message() << "\n";
        return;
    }

    // Read a message
    do_read();
}


void ControlSessionWs::do_read()
{
    // Read a message into our buffer
    auto self(shared_from_this());
    ws_.async_read(buffer_, [this, self](beast::error_code ec, std::size_t bytes_transferred) { on_read(ec, bytes_transferred); });
}


void ControlSessionWs::on_read(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This indicates that the session was closed
    if (ec == websocket::error::closed)
    {
        LOG(INFO) << "ControlSessionWs session closed\n";
        return;
    }

    if (ec)
    {
        LOG(ERROR) << "ControlSessionWs::on_read error: " << ec.message() << "\n";
        return;
    }

    std::string line{boost::beast::buffers_to_string(buffer_.data())};
    if (!line.empty())
    {
        LOG(INFO) << "received: " << line << "\n";
        if ((message_receiver_ != nullptr) && !line.empty())
            message_receiver_->onMessageReceived(this, line);
    }
    buffer_.consume(bytes_transferred);
    do_read();
}


void ControlSessionWs::stop()
{
}


void ControlSessionWs::sendAsync(const std::string& message)
{
    auto self(shared_from_this());
    ws_.async_write(boost::asio::buffer(message), [this, self](std::error_code ec, std::size_t length) {
        if (ec)
        {
            LOG(ERROR) << "Error while writing to control socket: " << ec.message() << "\n";
        }
        else
        {
            LOG(DEBUG) << "Wrote " << length << " bytes to control socket\n";
        }
    });
}


bool ControlSessionWs::send(const std::string& message)
{
    boost::system::error_code ec;
    ws_.write(boost::asio::buffer(message), ec);
    return !ec;
}
