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

// prototype/interface header file
#include "stream_server.hpp"

// local headers
#include "common/aixlog.hpp"
#include "config.hpp"
#include "stream_session_tcp.hpp"
#ifdef HAS_LIBRIST

#include "common/message/server_settings.hpp"
#include "common/message/codec_header.hpp"
#include "common/message/time.hpp"
#endif

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;
using namespace streamreader;

using json = nlohmann::json;

static constexpr auto LOG_TAG = "StreamServer";

StreamServer::StreamServer(boost::asio::io_context& io_context, ServerSettings serverSettings, StreamMessageReceiver* messageReceiver)
    : io_context_(io_context), config_timer_(io_context), settings_(std::move(serverSettings)), messageReceiver_(messageReceiver)
#ifdef HAS_LIBRIST
    , active_pcm_stream_(nullptr)
#endif
{
}


StreamServer::~StreamServer() = default;


void StreamServer::cleanup()
{
    auto new_end = std::remove_if(sessions_.begin(), sessions_.end(), [](const std::weak_ptr<StreamSession>& session) { return session.expired(); });
    auto count = distance(new_end, sessions_.end());
    if (count > 0)
    {
        LOG(INFO, LOG_TAG) << "Removing " << count << " inactive session(s), active sessions: " << sessions_.size() - count << "\n";
        sessions_.erase(new_end, sessions_.end());
    }
}


void StreamServer::addSession(const std::shared_ptr<StreamSession>& session)
{
    session->setMessageReceiver(this);
    session->setBufferMs(settings_.stream.bufferMs);
    session->start();

    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    sessions_.emplace_back(session);
    cleanup();
}


void StreamServer::onChunkEncoded(const PcmStream* pcmStream, bool isDefaultStream, const std::shared_ptr<msg::PcmChunk>& chunk, double /*duration*/)
{
    // LOG(DEBUG, LOG_TAG) << "*** AUDIO CHUNK *** onChunkEncoded: stream=" << pcmStream->getName() << ", isDefault=" << isDefaultStream << ", chunkSize=" << chunk->payloadSize << "\n";
    shared_const_buffer buffer(*chunk);

    // make a copy of the sessions to avoid that a session get's deleted
    std::vector<std::shared_ptr<StreamSession>> sessions;
    {
        std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
        for (const auto& session : sessions_)
            if (auto s = session.lock())
                sessions.push_back(s);
    }

    for (const auto& session : sessions)
    {
        if (!settings_.stream.sendAudioToMutedClients)
        {
            std::lock_guard<std::mutex> lock(Config::instance().getMutex());
            GroupPtr group = Config::instance().getGroupFromClient(session->clientId);
            if (group)
            {
                if (group->muted)
                {
                    continue;
                }
                else
                {
                    ClientInfoPtr client = group->getClient(session->clientId);
                    if (client && client->config.volume.muted)
                        continue;
                }
            }
        }

        LOG(DEBUG, LOG_TAG) << "*** SESSION CHECK *** clientId=" << session->clientId 
                       << ", hasPcmStream=" << (session->pcmStream() != nullptr)
                       << ", isDefaultStream=" << isDefaultStream 
                       << ", pcmStreamMatch=" << (session->pcmStream().get() == pcmStream) << "\n";
                       
        if (!session->pcmStream() && isDefaultStream) //->getName() == "default")
        {
            LOG(DEBUG, LOG_TAG) << "*** SENDING AUDIO *** to " << session->clientId << " (default stream path)\n";
            session->send(buffer);
        }
        else if (session->pcmStream().get() == pcmStream)
        {
            LOG(DEBUG, LOG_TAG) << "*** SENDING AUDIO *** to " << session->clientId << " (matched stream path)\n";
            session->send(buffer);
        }
        else
        {
            LOG(DEBUG, LOG_TAG) << "*** SKIPPING AUDIO *** for " << session->clientId << " (no stream match)\n";
        }
    }

#ifdef HAS_LIBRIST
    // Send audio via RIST transport (parallel to TCP/WebSocket sessions)
    if (rist_transport_ && isDefaultStream)
    {
        // Update active stream reference for CodecHeader
        active_pcm_stream_ = const_cast<streamreader::PcmStream*>(pcmStream);
        rist_transport_->sendAudioChunk(chunk);
    }
#endif
}


void StreamServer::onMessageReceived(const std::shared_ptr<StreamSession>& streamSession, const msg::BaseMessage& baseMessage, char* buffer)
{
    try
    {
        if (messageReceiver_ != nullptr)
            messageReceiver_->onMessageReceived(streamSession, baseMessage, buffer);
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Server::onMessageReceived exception: " << e.what() << ", message type: " << baseMessage.type << "\n";
        auto session = getStreamSession(streamSession.get());
        session->stop();
    }
}


void StreamServer::onDisconnect(StreamSession* streamSession)
{
    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    session_ptr session = getStreamSession(streamSession);

    if (session == nullptr)
        return;

    LOG(INFO, LOG_TAG) << "onDisconnect: " << session->clientId << "\n";
    LOG(DEBUG, LOG_TAG) << "sessions: " << sessions_.size() << "\n";
    sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                   [streamSession](const std::weak_ptr<StreamSession>& session)
    {
        auto s = session.lock();
        return s.get() == streamSession;
    }),
                    sessions_.end());
    LOG(DEBUG, LOG_TAG) << "sessions: " << sessions_.size() << "\n";
    if (messageReceiver_ != nullptr)
        messageReceiver_->onDisconnect(streamSession);
    cleanup();
}


session_ptr StreamServer::getStreamSession(StreamSession* streamSession) const
{
    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);

    for (const auto& session : sessions_)
    {
        if (auto s = session.lock())
            if (s.get() == streamSession)
                return s;
    }
    return nullptr;
}


session_ptr StreamServer::getStreamSession(const std::string& clientId) const
{
    //	LOG(INFO, LOG_TAG) << "getStreamSession: " << mac << "\n";
    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    for (const auto& session : sessions_)
    {
        if (auto s = session.lock())
            if (s->clientId == clientId)
                return s;
    }
    return nullptr;
}


void StreamServer::startAccept()
{
    auto accept_handler = [this](error_code ec, tcp::socket socket)
    {
        if (!ec)
            handleAccept(std::move(socket));
        else
            LOG(ERROR, LOG_TAG) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    for (auto& acceptor : acceptor_)
        acceptor->async_accept(accept_handler);
}


void StreamServer::handleAccept(tcp::socket socket)
{
    try
    {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        /// experimental: turn on tcp::no_delay
        socket.set_option(tcp::no_delay(true));

        LOG(NOTICE, LOG_TAG) << "StreamServer::NewConnection: " << socket.remote_endpoint().address().to_string() << "\n";
        shared_ptr<StreamSession> session = make_shared<StreamSessionTcp>(this, settings_, std::move(socket));
        addSession(session);
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception in StreamServer::handleAccept: " << e.what() << "\n";
    }
    startAccept();
}


void StreamServer::start()
{
    for (const auto& address : settings_.stream.bind_to_address)
    {
        try
        {
            LOG(INFO, LOG_TAG) << "Creating stream acceptor for address: " << address << ", port: " << settings_.stream.port << "\n";
            acceptor_.emplace_back(make_unique<tcp::acceptor>(boost::asio::make_strand(io_context_.get_executor()),
                                                              tcp::endpoint(boost::asio::ip::make_address(address), settings_.stream.port)));
        }
        catch (const boost::system::system_error& e)
        {
            LOG(ERROR, LOG_TAG) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
        }
    }

    startAccept();

#ifdef HAS_LIBRIST
    // Initialize RIST transport if enabled
    if (settings_.rist.enabled && !settings_.rist.bind_to_address.empty())
    {
        LOG(INFO, LOG_TAG) << "RIST is enabled, creating transport...\n";
        rist_transport_ = std::make_unique<RistTransport>(RistTransport::Mode::SERVER, this);
        
        // Configure for first address (like the old code)
        const std::string& address = settings_.rist.bind_to_address[0];
        if (rist_transport_->configureServer(address, settings_.rist.port))
        {
            if (rist_transport_->start())
            {
                LOG(INFO, LOG_TAG) << "Successfully started RIST transport for: " << address << ":" << settings_.rist.port << "\n";
            }
            else
            {
                LOG(ERROR, LOG_TAG) << "Failed to start RIST transport\n";
                rist_transport_.reset();
            }
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "Failed to configure RIST transport\n";
            rist_transport_.reset();
        }
    }
    else
    {
        LOG(INFO, LOG_TAG) << "RIST is disabled in configuration\n";
    }
#else
    LOG(INFO, LOG_TAG) << "RIST support not compiled in (HAS_LIBRIST not defined)\n";
#endif
}


void StreamServer::stop()
{
    for (auto& acceptor : acceptor_)
        acceptor->cancel();
    acceptor_.clear();

#ifdef HAS_LIBRIST
    // Stop RIST transport
    if (rist_transport_)
    {
        rist_transport_->stop();
        rist_transport_.reset();
    }
#endif

    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    cleanup();
    for (const auto& s : sessions_)
    {
        if (auto session = s.lock())
            session->stop();
    }
}

#ifdef HAS_LIBRIST
// Helper function to get RIST parameters from active stream or fallback to config
std::pair<uint32_t, uint32_t> StreamServer::getRistParameters() const
{
    // Try to get parameters from active stream first
    if (active_pcm_stream_)
    {
        const auto& uri = active_pcm_stream_->getUri();
        std::string min_str = uri.getQuery("recovery_length_min");
        std::string max_str = uri.getQuery("recovery_length_max");
        
        if (!min_str.empty() && !max_str.empty())
        {
            try
            {
                auto min_val = static_cast<uint32_t>(std::stoi(min_str));
                auto max_val = static_cast<uint32_t>(std::stoi(max_str));
                LOG(DEBUG, LOG_TAG) << "Using RIST parameters from stream URL: recovery_length_min=" << min_val 
                                   << ", recovery_length_max=" << max_val << "\n";
                return {min_val, max_val};
            }
            catch (const std::exception& e)
            {
                LOG(WARNING, LOG_TAG) << "Failed to parse RIST parameters from stream URL: " << e.what() << "\n";
            }
        }
    }
    
    // Fallback to config values
    LOG(DEBUG, LOG_TAG) << "Using RIST parameters from config: recovery_length_min=" << settings_.rist.recovery_length_min 
                       << ", recovery_length_max=" << settings_.rist.recovery_length_max << "\n";
    return {settings_.rist.recovery_length_min, settings_.rist.recovery_length_max};
}

void StreamServer::onRistMessageReceived(const msg::BaseMessage& baseMessage, const std::string& payload, 
                                        const char* payload_ptr /*, size_t payload_size, uint16_t vport */)
{
    // LOG(TRACE, LOG_TAG) << "RIST message received: type=" << baseMessage.type << ", vport=" << vport << "\n";
    
    // Handle RIST-specific messages directly (don't forward to session-based handler)
    try 
    {
        if (baseMessage.type == message_type::kHello && (!payload.empty() || payload_ptr))
        {
            // Handle Hello messages from RIST clients
            auto helloMsg = std::make_shared<msg::Hello>();
            // For zero-copy optimization: use raw pointer if payload is empty (large audio data)
            const char* data_ptr = payload.empty() ? payload_ptr : payload.data();
            /*
            if (payload.empty()) {
                LOG(TRACE, LOG_TAG) << "🔄 SERVER ZERO-COPY: Processing Hello with direct pointer (" << payload_size << " bytes)\n";
            } else {
                LOG(TRACE, LOG_TAG) << "📋 SERVER COPY: Processing Hello with copied data (" << payload.size() << " bytes)\n";
            }
            */
            helloMsg->deserialize(baseMessage, const_cast<char*>(data_ptr));
            
            LOG(DEBUG, LOG_TAG) << "RIST Hello received from client: " << helloMsg->getMacAddress() << "\n";
            
            // Send ServerSettings response via RIST transport
            if (rist_transport_)
            {
                msg::ServerSettings serverSettings;
                // Set refersTo field to correlate with Hello request
                serverSettings.refersTo = baseMessage.id;
                // Get RIST parameters from active stream or config
                auto [min_recovery, max_recovery] = getRistParameters();
                serverSettings.setRistRecoveryLengthMin(min_recovery);
                serverSettings.setRistRecoveryLengthMax(max_recovery);
                rist_transport_->sendMessage(RistTransport::VPORT_CONTROL, serverSettings);
                LOG(INFO, LOG_TAG) << "Sent ServerSettings to RIST client via control channel\n";
                
                // Send CodecHeader from active stream if available
                if (active_pcm_stream_)
                {
                    auto codecHeader = active_pcm_stream_->getHeader();
                    if (codecHeader)
                    {
                        rist_transport_->sendMessage(RistTransport::VPORT_AUDIO, *codecHeader);
                        LOG(DEBUG, LOG_TAG) << "Sent CodecHeader (" << codecHeader->payloadSize << " bytes) to RIST client via audio channel\n";
                    }
                    else
                    {
                        LOG(WARNING, LOG_TAG) << "No CodecHeader available from active stream\n";
                    }
                }
                else
                {
                    LOG(WARNING, LOG_TAG) << "No active stream available for CodecHeader\n";
                }
            }
        }
        else if (baseMessage.type == message_type::kTime && (!payload.empty() || payload_ptr))
        {
            // Handle Time messages for RIST clients
            auto timeMsg = std::make_shared<msg::Time>();
            // For zero-copy optimization: use raw pointer if payload is empty (large audio data)
            const char* data_ptr = payload.empty() ? payload_ptr : payload.data();
            /*
            if (payload.empty()) {
                LOG(TRACE, LOG_TAG) << "⏱️ SERVER ZERO-COPY: Processing Time with direct pointer (" << payload_size << " bytes)\n";
            } else {
                LOG(TRACE, LOG_TAG) << "📋 SERVER COPY: Processing Time with copied data (" << payload.size() << " bytes)\n";
            }
            */
            timeMsg->deserialize(baseMessage, const_cast<char*>(data_ptr));
            timeMsg->refersTo = timeMsg->id;
            timeMsg->latency = timeMsg->received - timeMsg->sent;
            
            // Send Time response back via RIST transport
            if (rist_transport_)
            {
                rist_transport_->sendMessage(RistTransport::VPORT_BACKCHANNEL, *timeMsg);
                // LOG(TRACE, LOG_TAG) << "Sent Time response via RIST backchannel\n";
            }
        }
        else
        {
            LOG(DEBUG, LOG_TAG) << "RIST message type " << baseMessage.type << " not handled (no action needed)\n";
        }
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Error processing RIST message: " << e.what() << "\n";
    }
}

void StreamServer::onRistClientConnected(const std::string& clientId)
{
    LOG(INFO, LOG_TAG) << "RIST client connected: " << clientId << "\n";
    
    // Send ServerSettings and CodecHeader like the testrist server
    if (rist_transport_)
    {
        // Create and send ServerSettings
        msg::ServerSettings serverSettings;
        // Get RIST parameters from active stream or config
        auto [min_recovery, max_recovery] = getRistParameters();
        serverSettings.setRistRecoveryLengthMin(min_recovery);
        serverSettings.setRistRecoveryLengthMax(max_recovery);
        rist_transport_->sendMessage(RistTransport::VPORT_CONTROL, serverSettings);
        LOG(INFO, LOG_TAG) << "Sent ServerSettings to RIST client: " << clientId << " with recovery_length_min=" << min_recovery << ", recovery_length_max=" << max_recovery << "\n";
        
        // Send CodecHeader from active stream if available
        if (active_pcm_stream_)
        {
            auto codecHeader = active_pcm_stream_->getHeader();
            if (codecHeader)
            {
                rist_transport_->sendMessage(RistTransport::VPORT_AUDIO, *codecHeader);
                LOG(DEBUG, LOG_TAG) << "Sent CodecHeader (" << codecHeader->payloadSize << " bytes) to RIST client: " << clientId << "\n";
            }
            else
            {
                LOG(WARNING, LOG_TAG) << "No CodecHeader available for RIST client: " << clientId << "\n";
            }
        }
        else
        {
            LOG(WARNING, LOG_TAG) << "No active PCM stream available for RIST client: " << clientId << " - CodecHeader will be sent with first audio chunk\n";
        }
    }
}

void StreamServer::onRistClientDisconnected(const std::string& clientId)
{
    LOG(INFO, LOG_TAG) << "RIST client disconnected: " << clientId << "\n";
    // Note: Unlike TCP sessions, RIST doesn't need explicit cleanup
}
#endif
