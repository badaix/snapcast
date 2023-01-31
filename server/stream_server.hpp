/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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

#ifndef STREAM_SERVER_HPP
#define STREAM_SERVER_HPP

// local headers
#include "common/message/codec_header.hpp"
#include "common/message/message.hpp"
#include "common/message/server_settings.hpp"
#include "common/queue.h"
#include "common/sample_format.hpp"
#include "control_server.hpp"
#include "jsonrpcpp.hpp"
#include "server_settings.hpp"
#include "stream_session.hpp"
#include "streamreader/stream_manager.hpp"

// 3rd party headers
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <vector>


using namespace streamreader;

using boost::asio::ip::tcp;
using acceptor_ptr = std::unique_ptr<tcp::acceptor>;
using session_ptr = std::shared_ptr<StreamSession>;


/// Forwars PCM data to the connected clients
/**
 * Reads PCM data from several StreamSessions
 * Accepts and holds client connections (StreamSession)
 * Receives (via the StreamMessageReceiver interface) and answers messages from the clients
 * Forwards PCM data to the clients
 */
class StreamServer : public StreamMessageReceiver
{
public:
    StreamServer(boost::asio::io_context& io_context, const ServerSettings& serverSettings, StreamMessageReceiver* messageReceiver = nullptr);
    virtual ~StreamServer();

    void start();
    void stop();

    /// Send a message to all connceted clients
    //	void send(const msg::BaseMessage* message);

    void addSession(std::shared_ptr<StreamSession> session);
    void onChunkEncoded(const PcmStream* pcmStream, bool isDefaultStream, std::shared_ptr<msg::PcmChunk> chunk, double duration);

    session_ptr getStreamSession(const std::string& clientId) const;
    session_ptr getStreamSession(StreamSession* session) const;

private:
    void startAccept();
    void handleAccept(tcp::socket socket);
    void cleanup();

    /// Implementation of StreamMessageReceiver
    void onMessageReceived(StreamSession* streamSession, const msg::BaseMessage& baseMessage, char* buffer) override;
    void onDisconnect(StreamSession* streamSession) override;

    mutable std::recursive_mutex sessionsMutex_;
    std::vector<std::weak_ptr<StreamSession>> sessions_;
    boost::asio::io_context& io_context_;
    std::vector<acceptor_ptr> acceptor_;
    boost::asio::steady_timer config_timer_;

    ServerSettings settings_;
    Queue<std::shared_ptr<msg::BaseMessage>> messages_;
    StreamMessageReceiver* messageReceiver_;
};



#endif
