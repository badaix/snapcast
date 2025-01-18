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
#include <boost/asio/read.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>

// standard headers
#include <cstdint>
#include <iostream>


using namespace std;

static constexpr auto LOG_TAG = "Connection";

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
    timer_.async_wait([this, self = shared_from_this()](boost::system::error_code ec)
    {
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
    : strand_(boost::asio::make_strand(io_context.get_executor())), resolver_(strand_), socket_(strand_), reqId_(1), server_(std::move(server))
{
    base_msg_size_ = base_message_.getSize();
    buffer_.resize(base_msg_size_);
}


ClientConnection::~ClientConnection()
{
    disconnect();
}


std::string ClientConnection::getMacAddress()
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
        LOG(INFO, LOG_TAG) << "Connecting to " << iter.endpoint() << "\n";
        socket_.connect(iter, ec);
        if (!ec || (ec == boost::system::errc::interrupted))
        {
            // We were successful or interrupted, e.g. by sig int
            break;
        }
    }

    if (ec)
        LOG(ERROR, LOG_TAG) << "Failed to connect to host '" << server_.host << "', error: " << ec.message() << "\n";
    else
        LOG(NOTICE, LOG_TAG) << "Connected to " << socket_.remote_endpoint().address().to_string() << "\n";

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


void ClientConnection::disconnect()
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


void ClientConnection::sendNext()
{
    auto& message = messages_.front();
    static boost::asio::streambuf streambuf;
    std::ostream stream(&streambuf);
    tv t;
    message.msg->sent = t;
    message.msg->serialize(stream);
    auto handler = message.handler;

    boost::asio::async_write(socket_, streambuf, [this, handler](boost::system::error_code ec, std::size_t length)
    {
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


void ClientConnection::getNextMessage(const MessageHandler<msg::BaseMessage>& handler)
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
            for (auto iter = pendingRequests_.begin(); iter != pendingRequests_.end(); ++iter)
            {
                auto request = *iter;
                if (auto req = request.lock())
                {
                    if (req->id() == base_message_.refersTo)
                    {
                        req->setValue(std::move(response));
                        pendingRequests_.erase(iter);
                        getNextMessage(handler);
                        return;
                    }
                }
            }

            if (handler)
                handler(ec, std::move(response));
        });
    });
}
