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
#include "authinfo.hpp"
#include "common/message/message.hpp"
#include "common/queue.hpp"
#include "control_requests.hpp"
#include "control_server.hpp"
#include "jsonrpcpp.hpp"
#include "server_settings.hpp"
#include "stream_server.hpp"
#include "stream_session.hpp"
#include "streamreader/stream_manager.hpp"


// 3rd party headers
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <memory>


using acceptor_ptr = std::unique_ptr<tcp::acceptor>;
using session_ptr = std::shared_ptr<StreamSession>;


/**
 * Receives and routes PcmStreams
 * Processes ControlSession messages
 */
class Server : public StreamMessageReceiver, public ControlMessageReceiver, public PcmStream::Listener
{
    friend class Request;

public:
    /// c'tor
    Server(boost::asio::io_context& io_context, const ServerSettings& serverSettings);
    /// d'tor
    virtual ~Server() = default;

    /// Start the server (control server, stream server and stream manager)
    void start();
    /// Stop the server (control server, stream server and stream manager)
    void stop();

private:
    /// Implementation of StreamMessageReceiver
    void onMessageReceived(StreamSession* streamSession, const msg::BaseMessage& baseMessage, char* buffer) override;
    void onDisconnect(StreamSession* streamSession) override;

    /// Implementation of ControllMessageReceiver
    void onMessageReceived(std::shared_ptr<ControlSession> controlSession, const std::string& message, const ResponseHandler& response_handler) override;
    void onNewSession(std::shared_ptr<ControlSession> session) override
    {
        std::ignore = session;
    };
    void onNewSession(std::shared_ptr<StreamSession> session) override;

    /// Implementation of PcmStream::Listener
    void onPropertiesChanged(const PcmStream* pcmStream, const Properties& properties) override;
    void onStateChanged(const PcmStream* pcmStream, ReaderState state) override;
    void onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk) override;
    void onChunkEncoded(const PcmStream* pcmStream, std::shared_ptr<msg::PcmChunk> chunk, double duration) override;
    void onResync(const PcmStream* pcmStream, double ms) override;

private:
    void processRequest(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const Request::OnResponse& on_response) const;
    /// Save the server state deferred to prevent blocking and lower disk io
    /// @param deferred the delay after the last call to saveConfig
    void saveConfig(const std::chrono::milliseconds& deferred = std::chrono::seconds(2));

    boost::asio::io_context& io_context_;
    boost::asio::steady_timer config_timer_;

    ServerSettings settings_;
    Queue<std::shared_ptr<msg::BaseMessage>> messages_;
    std::unique_ptr<ControlServer> controlServer_;
    std::unique_ptr<StreamServer> streamServer_;
    std::unique_ptr<StreamManager> streamManager_;
    ControlRequestFactory request_factory_;
};
