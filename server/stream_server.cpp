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
#include "stream_session_tcp_coordinated.hpp"
#include "common/buffer_pool.hpp"

// standard headers
#include <iomanip>
#include <thread>

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;
using namespace streamreader;

using json = nlohmann::json;

static constexpr auto LOG_TAG = "StreamServer";
static constexpr auto LOG_STATS_TAG = "StreamServerStats";

StreamServer::StreamServer(boost::asio::io_context& io_context, ServerSettings serverSettings, StreamMessageReceiver* messageReceiver)
    : io_context_(io_context), config_timer_(io_context), diagnostics_timer_(io_context), settings_(std::move(serverSettings)), messageReceiver_(messageReceiver)
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
        
        shared_ptr<StreamSession> session;
        if (settings_.stream.zerocopy)
        {
            LOG(INFO, LOG_TAG) << "Creating zerocopy-enabled session for " << socket.remote_endpoint().address().to_string() << "\n";
            session = make_shared<StreamSessionTcpCoordinated>(this, settings_, std::move(socket));
        }
        else
        {
            LOG(DEBUG, LOG_TAG) << "Creating regular TCP session for " << socket.remote_endpoint().address().to_string() << "\n";
            session = make_shared<StreamSessionTcp>(this, settings_, std::move(socket));
        }
        
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
    startDiagnosticsTimer();
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

void StreamServer::printZeroCopyDiagnostics(StreamSessionTcpCoordinated* /* coordinated_session */) const
{
    // Aggregate stats from all zerocopy sessions
    StreamSessionTcpCoordinated::ZeroCopyStats aggregated_stats = {};
    int zerocopy_session_count = 0;
    
    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    for (const auto& s : sessions_) {
        if (auto session = s.lock()) {
            if (auto zc_session = dynamic_cast<StreamSessionTcpCoordinated*>(session.get())) {
                auto stats = zc_session->getZeroCopyStats();
                aggregated_stats.zerocopy_attempts += stats.zerocopy_attempts;
                aggregated_stats.zerocopy_successful += stats.zerocopy_successful;
                aggregated_stats.zerocopy_bytes += stats.zerocopy_bytes;
                aggregated_stats.regular_sends += stats.regular_sends;
                aggregated_stats.regular_bytes += stats.regular_bytes;
                aggregated_stats.coordination_fallbacks += stats.coordination_fallbacks;
                aggregated_stats.pending_async_operations += stats.pending_async_operations;
                aggregated_stats.outstanding_zerocopy_buffers += stats.outstanding_zerocopy_buffers;
                aggregated_stats.completion_notifications_received += stats.completion_notifications_received;
                aggregated_stats.completion_notifications_missing += stats.completion_notifications_missing;
                aggregated_stats.buffers_completed_via_notifications += stats.buffers_completed_via_notifications;
                zerocopy_session_count++;
            }
        }
    }
    
    // Only print once for all sessions
    static bool already_printed = false;
    if (!already_printed && zerocopy_session_count > 0) {
        already_printed = true;
        
        LOG(INFO, LOG_STATS_TAG) << "=== Aggregated ZeroCopy Stats (All " << zerocopy_session_count << " Sessions) ==="
                           << "\n\tZC Attempts: " << aggregated_stats.zerocopy_attempts << ", "
                           << "\n\tZC Successful: " << aggregated_stats.zerocopy_successful << ", "
                           << "\n\tZC Bytes: " << aggregated_stats.zerocopy_bytes << ", "
                           << "\n\tRegular Sends: " << aggregated_stats.regular_sends << ", "
                           << "\n\tRegular Bytes: " << aggregated_stats.regular_bytes << ", "
                           << "\n\tCoordination Fallbacks: " << aggregated_stats.coordination_fallbacks << ", "
                           << "\n\tPending Async Operations: " << aggregated_stats.pending_async_operations << ", "
                           << "\n\tOutstanding ZC Buffers: " << aggregated_stats.outstanding_zerocopy_buffers << ", "
                           << "\n\tCompletion Notifications: " << aggregated_stats.completion_notifications_received << ", "
                           << "\n\tMissing Notifications: " << aggregated_stats.completion_notifications_missing << ", "
                           << std::fixed << std::setprecision(2)
                           << "\n\tZC Success Rate: " << aggregated_stats.zerocopy_percentage() << "%"
                           << "\n\tCompletion Reliability: " << aggregated_stats.completion_reliability() << "%\n";
        
        // Reset stats after reporting for all sessions
        for (const auto& s : sessions_) {
            if (auto session = s.lock()) {
                if (auto zc_session = dynamic_cast<StreamSessionTcpCoordinated*>(session.get())) {
                    zc_session->resetZeroCopyStats();
                }
            }
        }
        
        // Reset the flag for next reporting cycle
        std::thread([]{
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            already_printed = false;
        }).detach();
    }
}

void StreamServer::startDiagnosticsTimer()
{
    diagnostics_timer_.expires_after(std::chrono::seconds(30));
    diagnostics_timer_.async_wait([this](boost::system::error_code ec)
    {
        if (ec)
            return;
        
        // Only print diagnostics if there are active sessions
        {
            std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
            if (!sessions_.empty())
            {
                for (const auto& s : sessions_)
                {
                    if (auto session = s.lock())
                    {
                        // Handle zerocopy diagnostics
                        if (auto coordinated_session = std::dynamic_pointer_cast<StreamSessionTcpCoordinated>(session))
                        {
                            LOG(INFO, LOG_TAG) << "=== Periodic ZeroCopy Status (every 30s) ===\n";
                            printZeroCopyDiagnostics(coordinated_session.get());
                        }
                        
                        // Print buffer pool statistics for all session types (zerocopy and regular)
                        static bool buffer_stats_printed = false;
                        if (!buffer_stats_printed)
                        {
                            buffer_stats_printed = true;
                            auto buffer_stats = DynamicBufferPool::instance().getStats();
                            LOG(INFO, "BufferPool") << "=== Buffer Pool Stats ===" 
                                                   << "\n\tTotal Buffers: " << buffer_stats.total_buffers
                                                   << "\n\tAvailable Buffers: " << buffer_stats.available_buffers
                                                   << "\n\tBytes Allocated: " << buffer_stats.bytes_allocated
                                                   << "\n\tBuffers Created: " << buffer_stats.buffers_created  
                                                   << "\n\tBuffers Reused: " << buffer_stats.buffers_reused
                                                   << "\n\tCleanup Operations: " << buffer_stats.cleanup_operations << "\n";
                            
                            // Reset flag for next cycle
                            std::thread([]{   
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                buffer_stats_printed = false;
                            }).detach();
                        }
                    }
                }
            }
        }
        
        // Schedule next diagnostics check
        startDiagnosticsTimer();
    });
}
