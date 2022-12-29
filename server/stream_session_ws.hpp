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

#ifndef STREAM_SESSION_WS_HPP
#define STREAM_SESSION_WS_HPP

// local headers
#include "stream_session.hpp"

// 3rd party headers
#include <boost/asio/strand.hpp>
#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wdeprecated-copy-with-user-provided-copy"
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#include <boost/beast/core.hpp>
#pragma GCC diagnostic pop
#include <boost/beast/websocket.hpp>

// standard headers
#include <deque>

namespace beast = boost::beast; // from <boost/beast.hpp>
// namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>


/// Endpoint for a connected control client.
/**
 * Endpoint for a connected control client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the ControlMessageReceiver callback
 */
class StreamSessionWebsocket : public StreamSession
{
public:
    /// ctor. Received message from the client are passed to StreamMessageReceiver
    StreamSessionWebsocket(StreamMessageReceiver* receiver, websocket::stream<beast::tcp_stream>&& socket);
    ~StreamSessionWebsocket() override;
    void start() override;
    void stop() override;
    std::string getIP() override;

protected:
    // Websocket methods
    void sendAsync(const shared_const_buffer& buffer, const WriteHandler& handler) override;
    void on_read_ws(beast::error_code ec, std::size_t bytes_transferred);
    void do_read_ws();

    websocket::stream<beast::tcp_stream> ws_;

protected:
    beast::flat_buffer buffer_;
};



#endif
