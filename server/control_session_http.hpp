/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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

#pragma once


// local headers
#include "control_session.hpp"
#include "server_settings.hpp"

// 3rd party headers
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

// standard headers
#include <deque>
#include <optional>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>

using tcp_socket = boost::asio::ip::tcp::socket;
using ssl_socket = boost::asio::ssl::stream<tcp_socket>;

/// Endpoint for a connected control client.
/**
 * Endpoint for a connected control client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the ControlMessageReceiver callback
 */
class ControlSessionHttp : public ControlSession
{
public:
    /// c'tor for ssl sockets. Received message from the client are passed to ControlMessageReceiver
    ControlSessionHttp(ControlMessageReceiver* receiver, ssl_socket&& socket, const ServerSettings& settings);
    /// c'tor for tcp sockets
    ControlSessionHttp(ControlMessageReceiver* receiver, tcp_socket&& socket, const ServerSettings& settings);
    ~ControlSessionHttp() override;
    void start() override;
    void stop() override;

    /// Sends a message to the client (asynchronous)
    void sendAsync(const std::string& message) override;

private:
    /// HTTP on read callback
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    /// HTTP on write callback
    void on_write(beast::error_code ec, std::size_t bytes, bool close);

    /// Handle an incoming HTTP request
    template <class Body, class Allocator, class Send>
    void handle_request(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send);

    http::request<http::string_body> req_;
    std::optional<tcp_socket> tcp_socket_;
    std::optional<ssl_socket> ssl_socket_;
    beast::flat_buffer buffer_;
    ServerSettings settings_;
    std::deque<std::string> messages_;
    bool is_ssl_;
};
