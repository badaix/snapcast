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

#ifndef STREAM_SESSION_TCP_HPP
#define STREAM_SESSION_TCP_HPP

// local headers
#include "stream_session.hpp"

// 3rd party headers
#include <boost/asio/ip/tcp.hpp>

// standard headers

using boost::asio::ip::tcp;


/// Endpoint for a connected client.
/**
 * Endpoint for a connected client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the StreamMessageReceiver callback
 */
class StreamSessionTcp : public StreamSession
{
public:
    /// ctor. Received message from the client are passed to StreamMessageReceiver
    StreamSessionTcp(StreamMessageReceiver* receiver, tcp::socket&& socket);
    ~StreamSessionTcp() override;
    void start() override;
    void stop() override;
    std::string getIP() override;

protected:
    void read_next();
    void sendAsync(const shared_const_buffer& buffer, const WriteHandler& handler) override;

private:
    tcp::socket socket_;
};



#endif
