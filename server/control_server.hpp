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
#include "control_session.hpp"
#include "server_settings.hpp"

// 3rd party headers
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>

// standard headers
#include <memory>
#include <mutex>
#include <vector>


using boost::asio::ip::tcp;
using acceptor_ptr = std::unique_ptr<tcp::acceptor>;

/// Telnet like remote control
/**
 * Telnet like remote control
 */
class ControlServer : public ControlMessageReceiver
{
public:
    ControlServer(boost::asio::io_context& io_context, const ServerSettings& settings, ControlMessageReceiver* controlMessageReceiver = nullptr);
    virtual ~ControlServer();

    void start();
    void stop();

    /// Send a message to all connected clients
    void send(const std::string& message, const ControlSession* excludeSession = nullptr);

private:
    void startAccept();

    void cleanup();

    /// Implementation of ControlMessageReceiver
    void onMessageReceived(std::shared_ptr<ControlSession> session, const std::string& message, const ResponseHander& response_handler) override;
    void onNewSession(std::shared_ptr<ControlSession> session) override;
    void onNewSession(std::shared_ptr<StreamSession> session) override;

    mutable std::recursive_mutex session_mutex_;
    std::vector<std::weak_ptr<ControlSession>> sessions_;

    std::vector<acceptor_ptr> acceptor_;

    boost::asio::io_context& io_context_;
    boost::asio::ssl::context ssl_context_;
    ServerSettings::Tcp tcp_settings_;
    ServerSettings::Http http_settings_;
    ControlMessageReceiver* controlMessageReceiver_;
};
