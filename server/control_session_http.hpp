/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef CONTROL_SESSION_HTTP_HPP
#define CONTROL_SESSION_HTTP_HPP

// local headers
#include "control_session.hpp"

// 3rd party headers
#include <boost/beast/core.hpp>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <boost/beast/websocket.hpp>
#pragma GCC diagnostic pop
#else
#include <boost/beast/websocket.hpp>
#endif

// standard headers
#include <deque>

using boost::asio::ip::tcp;

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>


/// Endpoint for a connected control client.
/**
 * Endpoint for a connected control client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the ControlMessageReceiver callback
 */
class ControlSessionHttp : public ControlSession
{
public:
    /// ctor. Received message from the client are passed to ControlMessageReceiver
    ControlSessionHttp(ControlMessageReceiver* receiver, tcp::socket&& socket, const ServerSettings::Http& settings);
    ~ControlSessionHttp() override;
    void start() override;
    void stop() override;

    /// Sends a message to the client (asynchronous)
    void sendAsync(const std::string& message) override;

protected:
    // HTTP methods
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes, bool close);

    template <class Body, class Allocator, class Send>
    void handle_request(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    http::request<http::string_body> req_;

protected:
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    ServerSettings::Http settings_;
    std::deque<std::string> messages_;
};



#endif
