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

#ifndef CONTROL_SESSION_WS_HPP
#define CONTROL_SESSION_WS_HPP

#include "control_session.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

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
class ControlSessionWs : public ControlSession
{
public:
    /// ctor. Received message from the client are passed to MessageReceiver
    ControlSessionWs(ControlMessageReceiver* receiver, tcp::socket&& socket);
    ~ControlSessionWs() override;
    void start() override;
    void stop() override;

    /// Sends a message to the client (synchronous)
    bool send(const std::string& message) override;

    /// Sends a message to the client (asynchronous)
    void sendAsync(const std::string& message) override;

protected:
    websocket::stream<beast::tcp_stream> ws_;
    // beast::flat_buffer buffer_;
    // beast::multi_buffer b;
};



#endif
