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

#pragma once


// local headers
#include "stream_session.hpp"

// 3rd party headers
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#ifdef HAS_OPENSSL
#include <boost/beast/ssl.hpp>
#endif

// standard headers


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
using tcp_socket = boost::asio::ip::tcp::socket;
using tcp_websocket = websocket::stream<tcp_socket>;
#ifdef HAS_OPENSSL
using ssl_socket = boost::asio::ssl::stream<tcp_socket>;
using ssl_websocket = websocket::stream<ssl_socket>;
#endif

/// Endpoint for a connected control client.
/**
 * Endpoint for a connected control client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the ControlMessageReceiver callback
 */
class StreamSessionWebsocket : public StreamSession
{
public:
#ifdef HAS_OPENSSL
    /// c'tor for SSL. Received message from the client are passed to StreamMessageReceiver
    StreamSessionWebsocket(StreamMessageReceiver* receiver, const ServerSettings& server_settings, ssl_websocket&& ssl_ws);
#endif
    /// c'tor for TCP
    StreamSessionWebsocket(StreamMessageReceiver* receiver, const ServerSettings& server_settings, tcp_websocket&& tcp_ws);
    ~StreamSessionWebsocket() override;
    void start() override;
    void stop() override;
    std::string getIP() override;

private:
    /// Send message @p buffer and pass result to @p handler
    void sendAsync(const shared_const_buffer& buffer, WriteHandler&& handler) override;
    /// Read callback
    void on_read_ws(beast::error_code ec, std::size_t bytes_transferred);
    /// Read loop
    void do_read_ws();

    std::optional<tcp_websocket> tcp_ws_; ///< TCP websocket
#ifdef HAS_OPENSSL
    std::optional<ssl_websocket> ssl_ws_; ///< SSL websocket
#endif

    beast::flat_buffer buffer_; ///< read buffer
    bool is_ssl_;               ///< are we in SSL mode?
};
