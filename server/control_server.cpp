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
#include "control_server.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/json.hpp"
#include "control_session_http.hpp"
#include "control_session_tcp.hpp"
#include "server_settings.hpp"

// 3rd party headers

// standard headers
#include <iostream>


using namespace std;
using json = nlohmann::json;

static constexpr auto LOG_TAG = "ControlServer";


ControlServer::ControlServer(boost::asio::io_context& io_context, const ServerSettings& settings, ControlMessageReceiver* controlMessageReceiver)
    : io_context_(io_context), ssl_context_(boost::asio::ssl::context::sslv23), settings_(settings), controlMessageReceiver_(controlMessageReceiver)
{
    const ServerSettings::Ssl& ssl = settings.ssl;
    if (settings_.http.ssl_enabled)
    {
        ssl_context_.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                                 boost::asio::ssl::context::single_dh_use);
        if (!ssl.key_password.empty())
        {
            ssl_context_.set_password_callback([pw = ssl.key_password](size_t max_length, boost::asio::ssl::context_base::password_purpose purpose) -> string
            {
                LOG(DEBUG, LOG_TAG) << "getPassword, purpose: " << purpose << ", max length: " << max_length << "\n";
                return pw;
            });
        }
        if (!ssl.certificate.empty() && !ssl.certificate_key.empty())
        {
            ssl_context_.use_certificate_chain_file(ssl.certificate);
            ssl_context_.use_private_key_file(ssl.certificate_key, boost::asio::ssl::context::pem);
        }
        // ssl_context_.use_tmp_dh_file("dh4096.pem");
    }
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


void ControlServer::onMessageReceived(std::shared_ptr<ControlSession> session, const std::string& message, const ResponseHandler& response_handler)
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
    auto accept_handler = [this](error_code ec, tcp::socket socket)
    {
        if (!ec)
        {
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
            //	socket->set_option(boost::asio::ip::tcp::no_delay(false));
            auto port = socket.local_endpoint().port();
            LOG(NOTICE, LOG_TAG) << "New connection from: " << socket.remote_endpoint().address().to_string() << ", port: " << port << "\n";

            if (port == settings_.http.ssl_port)
            {
                auto session = make_shared<ControlSessionHttp>(this, ssl_socket(std::move(socket), ssl_context_), settings_);
                onNewSession(std::move(session));
            }
            else if (port == settings_.http.port)
            {
                auto session = make_shared<ControlSessionHttp>(this, std::move(socket), settings_);
                onNewSession(std::move(session));
            }
            else if (port == settings_.tcp.port)
            {
                auto session = make_shared<ControlSessionTcp>(this, std::move(socket), settings_);
                onNewSession(std::move(session));
            }
            else
            {
                LOG(ERROR, LOG_TAG) << "Port unknown, should not listen on this port?!?\n";
            }

            startAccept();
        }
        else
            LOG(ERROR, LOG_TAG) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    for (auto& acceptor : acceptor_)
        acceptor->async_accept(accept_handler);
}


void ControlServer::start()
{
    if (settings_.tcp.enabled)
    {
        for (const auto& address : settings_.tcp.bind_to_address)
        {
            try
            {
                LOG(INFO, LOG_TAG) << "Creating TCP acceptor for address: " << address << ", port: " << settings_.tcp.port << "\n";
                acceptor_.emplace_back(make_unique<tcp::acceptor>(boost::asio::make_strand(io_context_.get_executor()),
                                                                  tcp::endpoint(boost::asio::ip::make_address(address), settings_.tcp.port)));
            }
            catch (const boost::system::system_error& e)
            {
                LOG(ERROR, LOG_TAG) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
            }
        }
    }
    if (settings_.http.enabled || settings_.http.ssl_enabled)
    {
        if (settings_.http.enabled)
        {
            for (const auto& address : settings_.http.bind_to_address)
            {
                try
                {
                    LOG(INFO, LOG_TAG) << "Creating HTTP acceptor for address: " << address << ", port: " << settings_.http.port << "\n";
                    acceptor_.emplace_back(make_unique<tcp::acceptor>(boost::asio::make_strand(io_context_.get_executor()),
                                                                      tcp::endpoint(boost::asio::ip::make_address(address), settings_.http.port)));
                }
                catch (const boost::system::system_error& e)
                {
                    LOG(ERROR, LOG_TAG) << "error creating HTTP acceptor: " << e.what() << ", code: " << e.code() << "\n";
                }
            }
        }

        if (settings_.http.ssl_enabled)
        {
            for (const auto& address : settings_.http.ssl_bind_to_address)
            {
                try
                {
                    LOG(INFO, LOG_TAG) << "Creating HTTPS acceptor for address: " << address << ", port: " << settings_.http.ssl_port << "\n";
                    acceptor_.emplace_back(make_unique<tcp::acceptor>(boost::asio::make_strand(io_context_.get_executor()),
                                                                      tcp::endpoint(boost::asio::ip::make_address(address), settings_.http.ssl_port)));
                }
                catch (const boost::system::system_error& e)
                {
                    LOG(ERROR, LOG_TAG) << "error creating HTTP acceptor: " << e.what() << ", code: " << e.code() << "\n";
                }
            }
        }
    }

    startAccept();
}


void ControlServer::stop()
{
    for (auto& acceptor : acceptor_)
        acceptor->cancel();

    acceptor_.clear();

    std::lock_guard<std::recursive_mutex> mlock(session_mutex_);
    cleanup();
    for (const auto& s : sessions_)
    {
        if (auto session = s.lock())
            session->stop();
    }
}
