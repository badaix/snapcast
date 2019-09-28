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

#include "controlServer.h"
#include "aixlog.hpp"
#include "common/snapException.h"
#include "common/utils.h"
#include "config.h"
#include "control_session_tcp.hpp"
#include "control_session_ws.hpp"
#include "jsonrpcpp.hpp"
#include "message/time.h"

#include <iostream>

using namespace std;

using json = nlohmann::json;

// TODO: make this more generic, and less copy/paste
ControlServer::ControlServer(boost::asio::io_context* io_context, size_t port, ControlMessageReceiver* controlMessageReceiver)
    : acceptor_tcp_v4_(nullptr), acceptor_tcp_v6_(nullptr), acceptor_ws_v4_(nullptr), acceptor_ws_v6_(nullptr), io_context_(io_context), port_(port),
      controlMessageReceiver_(controlMessageReceiver)
{
}


ControlServer::~ControlServer()
{
    //	stop();
}


void ControlServer::cleanup()
{
    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (it->expired())
        {
            SLOG(ERROR) << "Session inactive. Removing\n";
            sessions_.erase(it++);
        }
        else
            ++it;
    }
}


void ControlServer::send(const std::string& message, const ControlSession* excludeSession)
{
    cleanup();
    for (auto s : sessions_)
    {
        if (auto session = s.lock())
        {
            if (session.get() != excludeSession)
                session->sendAsync(message);
        }
    }
}


void ControlServer::onMessageReceived(ControlSession* connection, const std::string& message)
{
    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    LOG(DEBUG) << "received: \"" << message << "\"\n";
    if (controlMessageReceiver_ != nullptr)
        controlMessageReceiver_->onMessageReceived(connection, message);
}


void ControlServer::startAccept()
{
    auto accept_handler_tcp = [this](error_code ec, tcp::socket socket) {
        if (!ec)
            handleAcceptTcp(std::move(socket));
        else
            LOG(ERROR) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    auto accept_handler_ws = [this](error_code ec, tcp::socket socket) {
        if (!ec)
            handleAcceptWs(std::move(socket));
        else
            LOG(ERROR) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    if (acceptor_tcp_v4_)
    {
        tcp::socket socket_v4(*io_context_);
        acceptor_tcp_v4_->async_accept(accept_handler_tcp);
    }
    if (acceptor_tcp_v6_)
    {
        tcp::socket socket_v6(*io_context_);
        acceptor_tcp_v6_->async_accept(accept_handler_tcp);
    }
    if (acceptor_ws_v4_)
    {
        tcp::socket socket_v4(*io_context_);
        acceptor_ws_v4_->async_accept(accept_handler_ws);
    }
    if (acceptor_ws_v6_)
    {
        tcp::socket socket_v6(*io_context_);
        acceptor_ws_v6_->async_accept(accept_handler_ws);
    }
}


void ControlServer::handleAcceptTcp(tcp::socket socket)
{
    try
    {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        //	socket->set_option(boost::asio::ip::tcp::no_delay(false));
        SLOG(NOTICE) << "ControlServer::NewConnection: " << socket.remote_endpoint().address().to_string() << endl;
        shared_ptr<ControlSessionTcp> session = make_shared<ControlSessionTcp>(this, std::move(socket));
        {
            std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
            session->start();
            sessions_.emplace_back(session);
            cleanup();
        }
    }
    catch (const std::exception& e)
    {
        SLOG(ERROR) << "Exception in ControlServer::handleAccept: " << e.what() << endl;
    }
    startAccept();
}


void ControlServer::handleAcceptWs(tcp::socket socket)
{
    try
    {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        //	socket->set_option(boost::asio::ip::tcp::no_delay(false));
        SLOG(NOTICE) << "ControlServer::NewConnection: " << socket.remote_endpoint().address().to_string() << endl;
        shared_ptr<ControlSessionWs> session = make_shared<ControlSessionWs>(this, std::move(socket));
        {
            std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
            session->start();
            sessions_.emplace_back(session);
            cleanup();
        }
    }
    catch (const std::exception& e)
    {
        SLOG(ERROR) << "Exception in ControlServer::handleAccept: " << e.what() << endl;
    }
    startAccept();
}


void ControlServer::start()
{
    bool is_v6_only(true);
    tcp::endpoint endpoint_tcp_v6(tcp::v6(), port_);
    try
    {
        acceptor_tcp_v6_ = make_shared<tcp::acceptor>(*io_context_, endpoint_tcp_v6);
        boost::system::error_code ec;
        acceptor_tcp_v6_->set_option(boost::asio::ip::v6_only(false), ec);
        boost::asio::ip::v6_only option;
        acceptor_tcp_v6_->get_option(option);
        is_v6_only = option.value();
        LOG(DEBUG) << "IPv6 only: " << is_v6_only << "\n";
    }
    catch (const boost::system::system_error& e)
    {
        LOG(ERROR) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
    }

    if (!acceptor_tcp_v6_ || is_v6_only)
    {
        tcp::endpoint endpoint_v4(tcp::v4(), port_);
        try
        {
            acceptor_tcp_v4_ = make_shared<tcp::acceptor>(*io_context_, endpoint_v4);
        }
        catch (const boost::system::system_error& e)
        {
            LOG(ERROR) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
        }
    }

    tcp::endpoint endpoint_ws_v6(tcp::v6(), 8080);
    try
    {
        acceptor_ws_v6_ = make_shared<tcp::acceptor>(*io_context_, endpoint_ws_v6);
        boost::system::error_code ec;
        acceptor_ws_v6_->set_option(boost::asio::ip::v6_only(false), ec);
        boost::asio::ip::v6_only option;
        acceptor_ws_v6_->get_option(option);
        is_v6_only = option.value();
        LOG(DEBUG) << "IPv6 only: " << is_v6_only << "\n";
    }
    catch (const boost::system::system_error& e)
    {
        LOG(ERROR) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
    }

    if (!acceptor_ws_v6_ || is_v6_only)
    {
        tcp::endpoint endpoint_v4(tcp::v4(), port_);
        try
        {
            acceptor_ws_v4_ = make_shared<tcp::acceptor>(*io_context_, endpoint_v4);
        }
        catch (const boost::system::system_error& e)
        {
            LOG(ERROR) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
        }
    }
    startAccept();
}


void ControlServer::stop()
{
    if (acceptor_tcp_v4_)
    {
        acceptor_tcp_v4_->cancel();
        acceptor_tcp_v4_ = nullptr;
    }
    if (acceptor_tcp_v6_)
    {
        acceptor_tcp_v6_->cancel();
        acceptor_tcp_v6_ = nullptr;
    }
    if (acceptor_ws_v4_)
    {
        acceptor_ws_v4_->cancel();
        acceptor_ws_v4_ = nullptr;
    }
    if (acceptor_ws_v6_)
    {
        acceptor_ws_v6_->cancel();
        acceptor_ws_v6_ = nullptr;
    }
    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    for (auto s : sessions_)
    {
        if (auto session = s.lock())
            session->stop();
    }
}
