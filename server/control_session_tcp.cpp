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

#include "control_session_tcp.hpp"
#include "aixlog.hpp"
#include "message/pcmChunk.h"
#include <iostream>
#include <mutex>

using namespace std;



ControlSessionTcp::ControlSessionTcp(ControlMessageReceiver* receiver, tcp::socket&& socket) : ControlSession(receiver, std::move(socket))
{
}


ControlSessionTcp::~ControlSessionTcp()
{
    LOG(DEBUG) << "ControlSessionTcp::~ControlSessionTcp()\n";
    stop();
}


void ControlSessionTcp::do_read()
{
    const std::string delimiter = "\n";
    auto self(shared_from_this());
    asio::async_read_until(socket_, streambuf_, delimiter, [this, self, delimiter](const std::error_code& ec, std::size_t bytes_transferred) {
        if (ec)
        {
            LOG(ERROR) << "Error while reading from control socket: " << ec.message() << "\n";
            return;
        }

        // Extract up to the first delimiter.
        std::string line{buffers_begin(streambuf_.data()), buffers_begin(streambuf_.data()) + bytes_transferred - delimiter.length()};
        if (!line.empty())
        {
            if (line.back() == '\r')
                line.resize(line.size() - 1);
            LOG(INFO) << "received: " << line << "\n";
            if ((message_receiver_ != nullptr) && !line.empty())
                message_receiver_->onMessageReceived(this, line);
        }
        streambuf_.consume(bytes_transferred);
        do_read();
    });
}

void ControlSessionTcp::start()
{
    do_read();
}


void ControlSessionTcp::stop()
{
    LOG(DEBUG) << "ControlSession::stop\n";
    std::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
        LOG(ERROR) << "Error in socket shutdown: " << ec.message() << "\n";
    socket_.close(ec);
    if (ec)
        LOG(ERROR) << "Error in socket close: " << ec.message() << "\n";
    LOG(DEBUG) << "ControlSession ControlSession stopped\n";
}


void ControlSessionTcp::sendAsync(const std::string& message)
{
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(message + "\r\n"), [this, self](std::error_code ec, std::size_t length) {
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


bool ControlSessionTcp::send(const std::string& message)
{
    error_code ec;
    asio::write(socket_, asio::buffer(message + "\r\n"), ec);
    return !ec;
}
