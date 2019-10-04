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
#include "control_session_http.hpp"
#include "control_session_tcp.hpp"
#include "jsonrpcpp.hpp"
#include "message/time.h"

#include <iostream>

using namespace std;
using json = nlohmann::json;


ControlServer::ControlServer(boost::asio::io_context* io_context, size_t port, ControlMessageReceiver* controlMessageReceiver)
    : io_context_(io_context), port_(port), controlMessageReceiver_(controlMessageReceiver)
{
}


ControlServer::~ControlServer()
{
    stop();
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


std::string ControlServer::onMessageReceived(ControlSession* connection, const std::string& message)
{
    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    LOG(DEBUG) << "received: \"" << message << "\"\n";
    if (controlMessageReceiver_ != nullptr)
        return controlMessageReceiver_->onMessageReceived(connection, message);
    return "";
}


void ControlServer::startAccept()
{
    auto accept_handler_tcp = [this](error_code ec, tcp::socket socket) {
        if (!ec)
            handleAccept<ControlSessionTcp>(std::move(socket));
        else
            LOG(ERROR) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    auto accept_handler_http = [this](error_code ec, tcp::socket socket) {
        if (!ec)
            handleAccept<ControlSessionHttp>(std::move(socket));
        else
            LOG(ERROR) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    if (acceptor_tcp_.first)
        acceptor_tcp_.first->async_accept(accept_handler_tcp);

    if (acceptor_tcp_.second)
        acceptor_tcp_.second->async_accept(accept_handler_tcp);

    if (acceptor_http_.first)
        acceptor_http_.first->async_accept(accept_handler_http);

    if (acceptor_http_.second)
        acceptor_http_.second->async_accept(accept_handler_http);
}


template <typename SessionType>
void ControlServer::handleAccept(tcp::socket socket)
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
        shared_ptr<SessionType> session = make_shared<SessionType>(this, std::move(socket));
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

std::pair<acceptor_ptr, acceptor_ptr> ControlServer::createAcceptors(size_t port)
{
    bool is_v6_only(true);
    tcp::endpoint endpoint_tcp_v6(tcp::v6(), port);
    acceptor_ptr acceptor_v4;
    acceptor_ptr acceptor_v6;
    try
    {
        acceptor_v6 = make_unique<tcp::acceptor>(*io_context_, endpoint_tcp_v6);
        boost::system::error_code ec;
        acceptor_v6->set_option(boost::asio::ip::v6_only(true), ec);
        boost::asio::ip::v6_only option;
        acceptor_v6->get_option(option);
        is_v6_only = option.value();
        LOG(DEBUG) << "IPv6 only: " << is_v6_only << "\n";
    }
    catch (const boost::system::system_error& e)
    {
        LOG(ERROR) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
    }

    if (!acceptor_v6 || is_v6_only)
    {
        tcp::endpoint endpoint_v4(tcp::v4(), port);
        try
        {
            acceptor_v4 = make_unique<tcp::acceptor>(*io_context_, endpoint_v4);
        }
        catch (const boost::system::system_error& e)
        {
            LOG(ERROR) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
        }
    }
    return make_pair<acceptor_ptr, acceptor_ptr>(std::move(acceptor_v4), std::move(acceptor_v6));
}

void ControlServer::start()
{
    // TODO: should be possible to be disabled
    acceptor_tcp_ = createAcceptors(port_);
    // TODO: make port configurable, should be possible to be disabled
    acceptor_http_ = createAcceptors(8080);
    startAccept();
}


void ControlServer::stop()
{
    auto cancel_accept = [](tcp::acceptor* acceptor) {
        if (acceptor)
        {
            acceptor->cancel();
            acceptor = nullptr;
        }
    };

    cancel_accept(acceptor_tcp_.first.get());
    cancel_accept(acceptor_tcp_.second.get());
    cancel_accept(acceptor_http_.first.get());
    cancel_accept(acceptor_http_.second.get());
    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    for (auto s : sessions_)
    {
        if (auto session = s.lock())
            session->stop();
    }
}
