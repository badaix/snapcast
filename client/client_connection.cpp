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
#include "client_connection.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/str_compat.hpp"

// 3rd party headers
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/system/detail/error_code.hpp>

// standard headers
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <utility>


using namespace std;
namespace http = beast::http; // from <boost/beast/http.hpp>

static constexpr auto LOG_TAG = "Connection";

static constexpr const char* WS_CLIENT_NAME = "Snapcast";


PendingRequest::PendingRequest(const boost::asio::strand<boost::asio::any_io_executor>& strand, uint16_t reqId, const MessageHandler<msg::BaseMessage>& handler)
    : id_(reqId), timer_(strand), strand_(strand), handler_(handler)
{
}

PendingRequest::~PendingRequest()
{
    handler_ = nullptr;
    timer_.cancel();
}

void PendingRequest::setValue(std::unique_ptr<msg::BaseMessage> value)
{
    boost::asio::post(strand_, [this, self = shared_from_this(), val = std::move(value)]() mutable
    {
        timer_.cancel();
        if (handler_)
            handler_({}, std::move(val));
    });
}

uint16_t PendingRequest::id() const
{
    return id_;
}

void PendingRequest::startTimer(const chronos::usec& timeout)
{
    timer_.expires_after(timeout);
    timer_.async_wait([this, me = weak_from_this()](boost::system::error_code ec)
    {
        auto self = me.lock();
        if (!self)
            return;

        if (!handler_)
            return;

        if (!ec)
        {
            // !ec => expired => timeout
            handler_(boost::asio::error::timed_out, nullptr);
            handler_ = nullptr;
        }
        else if (ec != boost::asio::error::operation_aborted)
        {
            // ec != aborted => not cancelled (in setValue)
            //   => should not happen, but who knows => pass the error to the handler
            handler_(ec, nullptr);
        }
    });
}

bool PendingRequest::operator<(const PendingRequest& other) const
{
    return (id_ < other.id());
}



ClientConnection::ClientConnection(boost::asio::io_context& io_context, ClientSettings::Server server)
    : strand_(boost::asio::make_strand(io_context.get_executor())), resolver_(strand_), reqId_(1), server_(std::move(server)),
      base_msg_size_(base_message_.getSize())
{
}


void ClientConnection::connect(const ResultHandler& handler)
{
    boost::system::error_code ec;
    LOG(INFO, LOG_TAG) << "Resolving host IP for: " << server_.host << "\n";
    auto iterator = resolver_.resolve(server_.host, cpt::to_string(server_.port), boost::asio::ip::resolver_query_base::numeric_service, ec);
    if (ec)
    {
        LOG(ERROR, LOG_TAG) << "Failed to resolve host '" << server_.host << "', error: " << ec.message() << "\n";
        handler(ec);
        return;
    }

    for (const auto& iter : iterator)
        LOG(DEBUG, LOG_TAG) << "Resolved IP: " << iter.endpoint().address().to_string() << "\n";

    for (const auto& iter : iterator)
    {
        LOG(INFO, LOG_TAG) << "Connecting to host: " << iter.endpoint() << ", port: " << server_.port << ", protocol: " << server_.protocol << "\n";
        ec = doConnect(iter.endpoint());
        if (!ec || (ec == boost::system::errc::interrupted))
        {
            // We were successful or interrupted, e.g. by sig int
            break;
        }
    }

    if (ec)
    {
        LOG(ERROR, LOG_TAG) << "Failed to connect to host '" << server_.host << "', error: " << ec.message() << "\n";
        disconnect();
    }
    else
        LOG(NOTICE, LOG_TAG) << "Connected to " << server_.host << "\n";

    handler(ec);

#if 0
    resolver_.async_resolve(query, host_, cpt::to_string(port_), [this, handler](const boost::system::error_code& ec, tcp::resolver::results_type results) {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Failed to resolve host '" << host_ << "', error: " << ec.message() << "\n";
            handler(ec);
            return;
        }

        resolver_.cancel();
        socket_.async_connect(*results, [this, handler](const boost::system::error_code& ec) {
            if (ec)
            {
                LOG(ERROR, LOG_TAG) << "Failed to connect to host '" << host_ << "', error: " << ec.message() << "\n";
                handler(ec);
                return;
            }

            LOG(NOTICE, LOG_TAG) << "Connected to " << socket_.remote_endpoint().address().to_string() << "\n";
            handler(ec);
            getNextMessage();
        });
    });
#endif
}


void ClientConnection::sendNext()
{
    auto& message = messages_.front();
    std::ostream stream(&streambuf_);
    tv t;
    message.msg->sent = t;
    message.msg->serialize(stream);
    ResultHandler handler = message.handler;

    write(streambuf_, [this, handler](boost::system::error_code ec, std::size_t length)
    {
        streambuf_.consume(length);
        if (ec)
            LOG(ERROR, LOG_TAG) << "Failed to send message, error: " << ec.message() << "\n";
        else
            LOG(TRACE, LOG_TAG) << "Wrote " << length << " bytes to socket\n";

        messages_.pop_front();
        if (handler)
            handler(ec);

        if (!messages_.empty())
            sendNext();
    });
}


void ClientConnection::send(const msg::message_ptr& message, const ResultHandler& handler)
{
    boost::asio::post(strand_, [this, message, handler]()
    {
        messages_.emplace_back(message, handler);
        if (messages_.size() > 1)
        {
            LOG(DEBUG, LOG_TAG) << "outstanding async_write\n";
            return;
        }
        sendNext();
    });
}


void ClientConnection::sendRequest(const msg::message_ptr& message, const chronos::usec& timeout, const MessageHandler<msg::BaseMessage>& handler)
{
    boost::asio::post(strand_, [this, message, timeout, handler]()
    {
        pendingRequests_.erase(
            std::remove_if(pendingRequests_.begin(), pendingRequests_.end(), [](const std::weak_ptr<PendingRequest>& request) { return request.expired(); }),
            pendingRequests_.end());
        unique_ptr<msg::BaseMessage> response(nullptr);
        static constexpr uint16_t max_req_id = 10000;
        if (++reqId_ >= max_req_id)
            reqId_ = 1;
        message->id = reqId_;
        auto request = make_shared<PendingRequest>(strand_, reqId_, handler);
        pendingRequests_.push_back(request);
        request->startTimer(timeout);
        send(message, [handler](const boost::system::error_code& ec)
        {
            if (ec)
                handler(ec, nullptr);
        });
    });
}


void ClientConnection::messageReceived(std::unique_ptr<msg::BaseMessage> message, const MessageHandler<msg::BaseMessage>& handler)
{
    for (auto iter = pendingRequests_.begin(); iter != pendingRequests_.end(); ++iter)
    {
        auto request = *iter;
        if (auto req = request.lock())
        {
            if (req->id() == base_message_.refersTo)
            {
                req->setValue(std::move(message));
                pendingRequests_.erase(iter);
                getNextMessage(handler);
                return;
            }
        }
    }

    if (handler)
        handler({}, std::move(message));
}


///////////////////////////////////// TCP /////////////////////////////////////

ClientConnectionTcp::ClientConnectionTcp(boost::asio::io_context& io_context, ClientSettings::Server server)
    : ClientConnection(io_context, std::move(server)), socket_(strand_)
{
    buffer_.resize(base_msg_size_);
}

ClientConnectionTcp::~ClientConnectionTcp()
{
    disconnect();
}


void ClientConnectionTcp::disconnect()
{
    LOG(DEBUG, LOG_TAG) << "Disconnecting\n";
    if (!socket_.is_open())
    {
        LOG(DEBUG, LOG_TAG) << "Not connected\n";
        return;
    }
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
        LOG(ERROR, LOG_TAG) << "Error in socket shutdown: " << ec.message() << "\n";
    socket_.close(ec);
    if (ec)
        LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
    boost::asio::post(strand_, [this]() { pendingRequests_.clear(); });
    LOG(DEBUG, LOG_TAG) << "Disconnected\n";
}


std::string ClientConnectionTcp::getMacAddress()
{
    std::string mac =
#ifndef WINDOWS
        ::getMacAddress(socket_.native_handle());
#else
        ::getMacAddress(socket_.local_endpoint().address().to_string());
#endif
    if (mac.empty())
        mac = "00:00:00:00:00:00";
    LOG(INFO, LOG_TAG) << "My MAC: \"" << mac << "\", socket: " << socket_.native_handle() << "\n";
    return mac;
}


void ClientConnectionTcp::getNextMessage(const MessageHandler<msg::BaseMessage>& handler)
{
    boost::asio::async_read(socket_, boost::asio::buffer(buffer_, base_msg_size_), [this, handler](boost::system::error_code ec, std::size_t length) mutable
    {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error reading message header of length " << length << ": " << ec.message() << "\n";
            if (handler)
                handler(ec, nullptr);
            return;
        }

        base_message_.deserialize(buffer_.data());
        tv t;
        base_message_.received = t;
        // LOG(TRACE, LOG_TAG) << "getNextMessage: " << base_message_.type << ", size: " << base_message_.size << ", id: " <<
        // base_message_.id << ", refers: " << base_message_.refersTo << "\n";
        if (base_message_.type > message_type::kLast)
        {
            LOG(ERROR, LOG_TAG) << "unknown message type received: " << base_message_.type << ", size: " << base_message_.size << "\n";
            if (handler)
                handler(boost::asio::error::invalid_argument, nullptr);
            return;
        }
        else if (base_message_.size > msg::max_size)
        {
            LOG(ERROR, LOG_TAG) << "received message of type " << base_message_.type << " to large: " << base_message_.size << "\n";
            if (handler)
                handler(boost::asio::error::invalid_argument, nullptr);
            return;
        }

        if (base_message_.size > buffer_.size())
            buffer_.resize(base_message_.size);

        boost::asio::async_read(socket_, boost::asio::buffer(buffer_, base_message_.size),
                                [this, handler](boost::system::error_code ec, std::size_t length) mutable
        {
            if (ec)
            {
                LOG(ERROR, LOG_TAG) << "Error reading message body of length " << length << ": " << ec.message() << "\n";
                if (handler)
                    handler(ec, nullptr);
                return;
            }

            auto response = msg::factory::createMessage(base_message_, buffer_.data());
            if (!response)
                LOG(WARNING, LOG_TAG) << "Failed to deserialize message of type: " << base_message_.type << "\n";

            messageReceived(std::move(response), handler);
        });
    });
}


boost::system::error_code ClientConnectionTcp::doConnect(boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> endpoint)
{
    boost::system::error_code ec;
    socket_.connect(endpoint, ec);
    return ec;
}


void ClientConnectionTcp::write(boost::asio::streambuf& buffer, WriteHandler&& write_handler)
{
    boost::asio::async_write(socket_, buffer, write_handler);
}


///////////////////////////////// Websockets //////////////////////////////////


ClientConnectionWs::ClientConnectionWs(boost::asio::io_context& io_context, ClientSettings::Server server)
    : ClientConnection(io_context, std::move(server)), tcp_ws_(strand_)
{
}


ClientConnectionWs::~ClientConnectionWs()
{
    disconnect();
}


tcp_websocket& ClientConnectionWs::getWs()
{
    // Looks like the websocket must be recreated after disconnect:
    // https://github.com/boostorg/beast/issues/2409#issuecomment-1103685782

    std::lock_guard lock(ws_mutex_);
    if (tcp_ws_.has_value())
        return tcp_ws_.value();

    tcp_ws_.emplace(strand_);
    return tcp_ws_.value();
}


void ClientConnectionWs::disconnect()
{
    LOG(DEBUG, LOG_TAG) << "Disconnecting\n";
    boost::system::error_code ec;

    if (getWs().is_open())
        getWs().close(websocket::close_code::normal, ec);
    // if (ec)
    //     LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
    if (getWs().next_layer().is_open())
    {
        getWs().next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        getWs().next_layer().close(ec);
    }
    boost::asio::post(strand_, [this]() { pendingRequests_.clear(); });
    tcp_ws_ = std::nullopt;
    LOG(DEBUG, LOG_TAG) << "Disconnected\n";
}


std::string ClientConnectionWs::getMacAddress()
{
    std::string mac =
#ifndef WINDOWS
        ::getMacAddress(getWs().next_layer().native_handle());
#else
        ::getMacAddress(getWs().next_layer().local_endpoint().address().to_string());
#endif
    if (mac.empty())
        mac = "00:00:00:00:00:00";
    LOG(INFO, LOG_TAG) << "My MAC: \"" << mac << "\", socket: " << getWs().next_layer().native_handle() << "\n";
    return mac;
}


void ClientConnectionWs::getNextMessage(const MessageHandler<msg::BaseMessage>& handler)
{
    getWs().async_read(buffer_, [this, handler](beast::error_code ec, std::size_t bytes_transferred) mutable
    {
        tv now;
        LOG(TRACE, LOG_TAG) << "on_read_ws, ec: " << ec << ", bytes_transferred: " << bytes_transferred << "\n";

        // This indicates that the session was closed
        if (ec == websocket::error::closed)
        {
            if (handler)
                handler(ec, nullptr);
            return;
        }

        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "ControlSessionWebsocket::on_read_ws error: " << ec.message() << "\n";
            if (handler)
                handler(ec, nullptr);
            return;
        }

        buffer_.consume(bytes_transferred);

        auto* data = static_cast<char*>(buffer_.data().data());
        base_message_.deserialize(data);

        base_message_.received = now;

        auto response = msg::factory::createMessage(base_message_, data + base_msg_size_);
        if (!response)
            LOG(WARNING, LOG_TAG) << "Failed to deserialize message of type: " << base_message_.type << "\n";
        else
            LOG(TRACE, LOG_TAG) << "getNextMessage: " << response->type << ", size: " << response->size << ", id: " << response->id
                                << ", refers: " << response->refersTo << "\n";

        messageReceived(std::move(response), handler);
    });
}


boost::system::error_code ClientConnectionWs::doConnect(boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> endpoint)
{
    boost::system::error_code ec;
    getWs().binary(true);
    getWs().next_layer().connect(endpoint, ec);
    if (ec.failed())
        return ec;

    // Set suggested timeout settings for the websocket
    getWs().set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    // Set a decorator to change the User-Agent of the handshake
    getWs().set_option(websocket::stream_base::decorator([](websocket::request_type& req) { req.set(http::field::user_agent, WS_CLIENT_NAME); }));

    // Perform the websocket handshake
    getWs().handshake(server_.host + ":" + std::to_string(server_.port), "/stream", ec);
    return ec;
}


void ClientConnectionWs::write(boost::asio::streambuf& buffer, WriteHandler&& write_handler)
{
    getWs().async_write(boost::asio::buffer(buffer.data()), write_handler);
}



/////////////////////////////// SSL Websockets ////////////////////////////////

#ifdef HAS_OPENSSL

ClientConnectionWss::ClientConnectionWss(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context, ClientSettings::Server server)
    : ClientConnection(io_context, std::move(server)), ssl_context_(ssl_context)
{
    getWs();
}


ssl_websocket& ClientConnectionWss::getWs()
{
    // Looks like the websocket must be recreated after disconnect:
    // https://github.com/boostorg/beast/issues/2409#issuecomment-1103685782

    std::lock_guard lock(ws_mutex_);
    if (ssl_ws_.has_value())
        return ssl_ws_.value();

    ssl_ws_.emplace(strand_, ssl_context_);
    if (server_.server_certificate.has_value())
    {
        ssl_ws_->next_layer().set_verify_mode(boost::asio::ssl::verify_peer);
        ssl_ws_->next_layer().set_verify_callback([](bool preverified, boost::asio::ssl::verify_context& ctx)
        {
            // The verify callback can be used to check whether the certificate that is
            // being presented is valid for the peer. For example, RFC 2818 describes
            // the steps involved in doing this for HTTPS. Consult the OpenSSL
            // documentation for more details. Note that the callback is called once
            // for each certificate in the certificate chain, starting from the root
            // certificate authority.

            // In this example we will simply print the certificate's subject name.
            char subject_name[256];
            X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
            X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
            LOG(INFO, LOG_TAG) << "Verifying cert: '" << subject_name << "', pre verified: " << preverified << "\n";

            return preverified;
        });
    }
    return ssl_ws_.value();
}


ClientConnectionWss::~ClientConnectionWss()
{
    disconnect();
}


void ClientConnectionWss::disconnect()
{
    LOG(DEBUG, LOG_TAG) << "Disconnecting\n";
    boost::system::error_code ec;

    if (getWs().is_open())
        getWs().close(websocket::close_code::normal, ec);
    // if (ec)
    //     LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
    if (getWs().next_layer().lowest_layer().is_open())
    {
        getWs().next_layer().lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        getWs().next_layer().lowest_layer().close(ec);
    }

    boost::asio::post(strand_, [this]() { pendingRequests_.clear(); });
    ssl_ws_ = std::nullopt;
    LOG(DEBUG, LOG_TAG) << "Disconnected\n";
}


std::string ClientConnectionWss::getMacAddress()
{
    std::string mac =
#ifndef WINDOWS
        ::getMacAddress(getWs().next_layer().lowest_layer().native_handle());
#else
        ::getMacAddress(getWs().next_layer().lowest_layer().local_endpoint().address().to_string());
#endif
    if (mac.empty())
        mac = "00:00:00:00:00:00";
    LOG(INFO, LOG_TAG) << "My MAC: \"" << mac << "\", socket: " << getWs().next_layer().lowest_layer().native_handle() << "\n";
    return mac;
}


void ClientConnectionWss::getNextMessage(const MessageHandler<msg::BaseMessage>& handler)
{
    getWs().async_read(buffer_, [this, handler](beast::error_code ec, std::size_t bytes_transferred) mutable
    {
        tv now;
        LOG(TRACE, LOG_TAG) << "on_read_ws, ec: " << ec << ", bytes_transferred: " << bytes_transferred << "\n";

        // This indicates that the session was closed
        if (ec == websocket::error::closed)
        {
            if (handler)
                handler(ec, nullptr);
            return;
        }

        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "ControlSessionWebsocket::on_read_ws error: " << ec.message() << "\n";
            if (handler)
                handler(ec, nullptr);
            return;
        }

        buffer_.consume(bytes_transferred);

        auto* data = static_cast<char*>(buffer_.data().data());
        base_message_.deserialize(data);

        base_message_.received = now;

        auto response = msg::factory::createMessage(base_message_, data + base_msg_size_);
        if (!response)
            LOG(WARNING, LOG_TAG) << "Failed to deserialize message of type: " << base_message_.type << "\n";
        else
            LOG(TRACE, LOG_TAG) << "getNextMessage: " << response->type << ", size: " << response->size << ", id: " << response->id
                                << ", refers: " << response->refersTo << "\n";

        messageReceived(std::move(response), handler);
    });
}


boost::system::error_code ClientConnectionWss::doConnect(boost::asio::ip::basic_endpoint<boost::asio::ip::tcp> endpoint)
{
    boost::system::error_code ec;
    getWs().binary(true);
    beast::get_lowest_layer(getWs()).connect(endpoint, ec);
    if (ec.failed())
        return ec;

    // Set a timeout on the operation
    // beast::get_lowest_layer(getWs()).expires_after(std::chrono::seconds(30));

    // Set suggested timeout settings for the websocket
    getWs().set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    // Set SNI Hostname (many hosts need this to handshake successfully)
    if (!SSL_set_tlsext_host_name(getWs().next_layer().native_handle(), server_.host.c_str()))
    {
        LOG(ERROR, LOG_TAG) << "Failed to set SNI Hostname\n";
        return boost::system::error_code(static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category());
    }

    // Perform the SSL handshake
    getWs().next_layer().handshake(boost::asio::ssl::stream_base::client, ec);
    if (ec.failed())
        return ec;

    // Set a decorator to change the User-Agent of the handshake
    getWs().set_option(websocket::stream_base::decorator([](websocket::request_type& req) { req.set(http::field::user_agent, WS_CLIENT_NAME); }));

    // Perform the websocket handshake
    getWs().handshake(server_.host + ":" + std::to_string(server_.port), "/stream", ec);

    return ec;
}


void ClientConnectionWss::write(boost::asio::streambuf& buffer, WriteHandler&& write_handler)
{
    getWs().async_write(boost::asio::buffer(buffer.data()), write_handler);
}

#endif // HAS_OPENSSL
