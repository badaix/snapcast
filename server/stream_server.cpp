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

// prototype/interface header file
#include "stream_server.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/message/client_info.hpp"
#include "common/message/hello.hpp"
#include "common/message/time.hpp"
#include "config.hpp"
#include "stream_session_tcp.hpp"

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;
using namespace streamreader;

using json = nlohmann::json;

static constexpr auto LOG_TAG = "StreamServer";

StreamServer::StreamServer(boost::asio::io_context& io_context, const ServerSettings& serverSettings, StreamMessageReceiver* messageReceiver)
    : io_context_(io_context), config_timer_(io_context), settings_(serverSettings), messageReceiver_(messageReceiver)
{
}


StreamServer::~StreamServer() = default;


void StreamServer::cleanup()
{
    auto new_end = std::remove_if(sessions_.begin(), sessions_.end(), [](std::weak_ptr<StreamSession> session) { return session.expired(); });
    auto count = distance(new_end, sessions_.end());
    if (count > 0)
    {
        LOG(INFO, LOG_TAG) << "Removing " << count << " inactive session(s), active sessions: " << sessions_.size() - count << "\n";
        sessions_.erase(new_end, sessions_.end());
    }
}


void StreamServer::addSession(std::shared_ptr<StreamSession> session)
{
    session->setMessageReceiver(this);
    session->setBufferMs(settings_.stream.bufferMs);
    session->start();

    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    sessions_.emplace_back(std::move(session));
    cleanup();
}


void StreamServer::onChunkEncoded(const PcmStream* pcmStream, bool isDefaultStream, std::shared_ptr<msg::PcmChunk> chunk, double /*duration*/)
{
    // LOG(TRACE, LOG_TAG) << "onChunkRead (" << pcmStream->getName() << "): " << duration << "ms\n";
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

        if (!session->pcmStream() && isDefaultStream) //->getName() == "default")
            session->send(buffer);
        else if (session->pcmStream().get() == pcmStream)
            session->send(buffer);
    }
}


void StreamServer::onMessageReceived(StreamSession* streamSession, const msg::BaseMessage& baseMessage, char* buffer)
{
    try
    {
        if (messageReceiver_ != nullptr)
            messageReceiver_->onMessageReceived(streamSession, baseMessage, buffer);
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Server::onMessageReceived exception: " << e.what() << ", message type: " << baseMessage.type << "\n";
        auto session = getStreamSession(streamSession);
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
                                   [streamSession](std::weak_ptr<StreamSession> session)
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

        LOG(NOTICE, LOG_TAG) << "StreamServer::NewConnection: " << socket.remote_endpoint().address().to_string() << endl;
        shared_ptr<StreamSession> session = make_shared<StreamSessionTcp>(this, std::move(socket));
        addSession(session);
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception in StreamServer::handleAccept: " << e.what() << endl;
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
                                                              tcp::endpoint(boost::asio::ip::address::from_string(address), settings_.stream.port)));
        }
        catch (const boost::system::system_error& e)
        {
            LOG(ERROR, LOG_TAG) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
        }
    }

    startAccept();
}


void StreamServer::stop()
{
    for (auto& acceptor : acceptor_)
        acceptor->cancel();
    acceptor_.clear();

    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    cleanup();
    for (const auto& s : sessions_)
    {
        if (auto session = s.lock())
            session->stop();
    }
}
