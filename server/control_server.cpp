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
#include "control_server.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/message/time.hpp"
#include "common/snap_exception.hpp"
#include "common/utils.hpp"
#include "config.hpp"
#include "control_session_http.hpp"
#include "control_session_tcp.hpp"
#include "jsonrpcpp.hpp"

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;
using json = nlohmann::json;

static constexpr auto LOG_TAG = "ControlServer";


ControlServer::ControlServer(boost::asio::io_context& io_context, const ServerSettings::Tcp& tcp_settings, const ServerSettings::Http& http_settings,
                             ControlMessageReceiver* controlMessageReceiver)
    : io_context_(io_context), tcp_settings_(tcp_settings), http_settings_(http_settings), controlMessageReceiver_(controlMessageReceiver)
{
}


ControlServer::~ControlServer()
{
    stop();
}


void ControlServer::cleanup()
{
    auto new_end = std::remove_if(sessions_.begin(), sessions_.end(), [](const std::weak_ptr<ControlSession>& session) { return session.expired(); });
    auto count = distance(new_end, sessions_.end());
    if (count > 0)
    {
        LOG(INFO, LOG_TAG) << "Removing " << count << " inactive session(s), active sessions: " << sessions_.size() - count << "\n";
        sessions_.erase(new_end, sessions_.end());
    }
}


void ControlServer::send(const std::string& message, const ControlSession* excludeSession)
{
    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    for (const auto& s : sessions_)
    {
        if (auto session = s.lock())
        {
            if (session.get() != excludeSession)
                session->sendAsync(message);
        }
    }
    cleanup();
}


void ControlServer::onMessageReceived(std::shared_ptr<ControlSession> session, const std::string& message, const ResponseHander& response_handler)
{
    // LOG(DEBUG, LOG_TAG) << "received: \"" << message << "\"\n";
    if (controlMessageReceiver_ != nullptr)
        controlMessageReceiver_->onMessageReceived(std::move(session), message, response_handler);
}


void ControlServer::onNewSession(shared_ptr<ControlSession> session)
{
    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    session->start();
    sessions_.emplace_back(std::move(session));
    cleanup();
}


void ControlServer::onNewSession(std::shared_ptr<StreamSession> session)
{
    if (controlMessageReceiver_ != nullptr)
        controlMessageReceiver_->onNewSession(std::move(session));
}


void ControlServer::startAccept()
{
    auto accept_handler_tcp = [this](error_code ec, tcp::socket socket)
    {
        if (!ec)
            handleAccept<ControlSessionTcp>(std::move(socket));
        else
            LOG(ERROR, LOG_TAG) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    auto accept_handler_http = [this](error_code ec, tcp::socket socket)
    {
        if (!ec)
            handleAccept<ControlSessionHttp>(std::move(socket), http_settings_);
        else
            LOG(ERROR, LOG_TAG) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    for (auto& acceptor : acceptor_tcp_)
        acceptor->async_accept(accept_handler_tcp);

    for (auto& acceptor : acceptor_http_)
        acceptor->async_accept(accept_handler_http);
}


template <typename SessionType, typename... Args>
void ControlServer::handleAccept(tcp::socket socket, Args&&... args)
{
    try
    {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        //	socket->set_option(boost::asio::ip::tcp::no_delay(false));
        LOG(NOTICE, LOG_TAG) << "ControlServer::NewConnection: " << socket.remote_endpoint().address().to_string() << endl;
        shared_ptr<SessionType> session = make_shared<SessionType>(this, std::move(socket), std::forward<Args>(args)...);
        onNewSession(std::move(session));
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception in ControlServer::handleAccept: " << e.what() << endl;
    }
    startAccept();
}



void ControlServer::start()
{
    if (tcp_settings_.enabled)
    {
        for (const auto& address : tcp_settings_.bind_to_address)
        {
            try
            {
                LOG(INFO, LOG_TAG) << "Creating TCP acceptor for address: " << address << ", port: " << tcp_settings_.port << "\n";
                acceptor_tcp_.emplace_back(make_unique<tcp::acceptor>(boost::asio::make_strand(io_context_.get_executor()),
                                                                      tcp::endpoint(boost::asio::ip::address::from_string(address), tcp_settings_.port)));
            }
            catch (const boost::system::system_error& e)
            {
                LOG(ERROR, LOG_TAG) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
            }
        }
    }
    if (http_settings_.enabled)
    {
        for (const auto& address : http_settings_.bind_to_address)
        {
            try
            {
                LOG(INFO, LOG_TAG) << "Creating HTTP acceptor for address: " << address << ", port: " << http_settings_.port << "\n";
                acceptor_http_.emplace_back(make_unique<tcp::acceptor>(boost::asio::make_strand(io_context_.get_executor()),
                                                                       tcp::endpoint(boost::asio::ip::address::from_string(address), http_settings_.port)));
            }
            catch (const boost::system::system_error& e)
            {
                LOG(ERROR, LOG_TAG) << "error creating HTTP acceptor: " << e.what() << ", code: " << e.code() << "\n";
            }
        }
    }

    startAccept();
}


void ControlServer::stop()
{
    for (auto& acceptor : acceptor_tcp_)
        acceptor->cancel();

    for (auto& acceptor : acceptor_http_)
        acceptor->cancel();

    acceptor_tcp_.clear();
    acceptor_http_.clear();

    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    cleanup();
    for (const auto& s : sessions_)
    {
        if (auto session = s.lock())
            session->stop();
    }
}
