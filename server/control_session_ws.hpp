/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#ifndef CONTROL_SESSION_WS_HPP
#define CONTROL_SESSION_WS_HPP

#include "control_session.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>


/// Endpoint for a connected control client.
/**
 * Endpoint for a connected control client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the ControlMessageReceiver callback
 */
class ControlSessionWebsocket : public ControlSession
{
public:
    /// ctor. Received message from the client are passed to MessageReceiver
    ControlSessionWebsocket(ControlMessageReceiver* receiver, boost::asio::io_context& ioc, websocket::stream<beast::tcp_stream>&& socket);
    ~ControlSessionWebsocket() override;
    void start() override;
    void stop() override;

    /// Sends a message to the client (asynchronous)
    void sendAsync(const std::string& message) override;

protected:
    // Websocket methods
    void on_read_ws(beast::error_code ec, std::size_t bytes_transferred);
    void do_read_ws();
    void send_next();

    websocket::stream<beast::tcp_stream> ws_;

protected:
    beast::flat_buffer buffer_;
    boost::asio::io_context::strand strand_;
    std::deque<std::string> messages_;
};



#endif
