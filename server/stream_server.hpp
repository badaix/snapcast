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

#ifndef STREAM_SERVER_H
#define STREAM_SERVER_H

#include <boost/asio.hpp>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include "common/queue.h"
#include "common/sampleFormat.h"
#include "control_server.hpp"
#include "jsonrpcpp.hpp"
#include "message/codecHeader.h"
#include "message/message.h"
#include "message/serverSettings.h"
#include "server_settings.hpp"
#include "stream_session.hpp"
#include "streamreader/streamManager.h"


using boost::asio::ip::tcp;
typedef std::shared_ptr<tcp::socket> socket_ptr;
typedef std::shared_ptr<StreamSession> session_ptr;


/// Forwars PCM data to the connected clients
/**
 * Reads PCM data using PipeStream, implements PcmListener to get the (encoded) PCM stream.
 * Accepts and holds client connections (StreamSession)
 * Receives (via the MessageReceiver interface) and answers messages from the clients
 * Forwards PCM data to the clients
 */
class StreamServer : public MessageReceiver, ControlMessageReceiver, PcmListener
{
public:
    StreamServer(boost::asio::io_context* io_context, const ServerSettings& serverSettings);
    virtual ~StreamServer();

    void start();
    void stop();

    /// Send a message to all connceted clients
    //	void send(const msg::BaseMessage* message);

    /// Clients call this when they receive a message. Implementation of MessageReceiver::onMessageReceived
    void onMessageReceived(StreamSession* connection, const msg::BaseMessage& baseMessage, char* buffer) override;
    void onDisconnect(StreamSession* connection) override;

    /// Implementation of ControllMessageReceiver::onMessageReceived, called by ControlServer::onMessageReceived
    std::string onMessageReceived(ControlSession* connection, const std::string& message) override;

    /// Implementation of PcmListener
    void onMetaChanged(const PcmStream* pcmStream) override;
    void onStateChanged(const PcmStream* pcmStream, const ReaderState& state) override;
    void onChunkRead(const PcmStream* pcmStream, msg::PcmChunk* chunk, double duration) override;
    void onResync(const PcmStream* pcmStream, double ms) override;

private:
    void startAccept();
    void handleAccept(tcp::socket socket);
    session_ptr getStreamSession(const std::string& mac) const;
    session_ptr getStreamSession(StreamSession* session) const;
    void ProcessRequest(const jsonrpcpp::request_ptr request, jsonrpcpp::entity_ptr& response, jsonrpcpp::notification_ptr& notification) const;
    mutable std::recursive_mutex sessionsMutex_;
    std::set<session_ptr> sessions_;
    boost::asio::io_context* io_context_;
    std::shared_ptr<tcp::acceptor> acceptor_v4_;
    std::shared_ptr<tcp::acceptor> acceptor_v6_;

    ServerSettings settings_;
    Queue<std::shared_ptr<msg::BaseMessage>> messages_;
    std::unique_ptr<ControlServer> controlServer_;
    std::unique_ptr<StreamManager> streamManager_;
};



#endif
