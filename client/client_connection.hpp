/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include "client_settings.hpp"
#include "common/time_defs.hpp"
#include "message/factory.hpp"
#include "message/message.hpp"

#include <atomic>
#include <boost/asio.hpp>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>


using boost::asio::ip::tcp;


class ClientConnection;

template <typename Message>
using MessageHandler = std::function<void(const boost::system::error_code&, std::unique_ptr<Message>)>;

/// Used to synchronize server requests (wait for server response)
class PendingRequest : public std::enable_shared_from_this<PendingRequest>
{
public:
    PendingRequest(boost::asio::io_context& io_context, boost::asio::io_context::strand& strand, uint16_t reqId,
                   const MessageHandler<msg::BaseMessage>& handler)
        : id_(reqId), timer_(io_context), strand_(strand), handler_(handler){};

    virtual ~PendingRequest()
    {
        handler_ = nullptr;
        timer_.cancel();
    }

    /// Set the response for the pending request and passes it to the handler
    /// @param value the response message
    void setValue(std::unique_ptr<msg::BaseMessage> value)
    {
        boost::asio::post(strand_, [this, self = shared_from_this(), val = std::move(value)]() mutable {
            timer_.cancel();
            if (handler_)
                handler_({}, std::move(val));
        });
    }

    /// @return the id of the request
    uint16_t id() const
    {
        return id_;
    }

    /// Start the timer for the request
    /// @param timeout the timeout to wait for the reception of the response
    void startTimer(const chronos::usec& timeout)
    {
        timer_.expires_after(timeout);
        timer_.async_wait(boost::asio::bind_executor(strand_, [this, self = shared_from_this()](boost::system::error_code ec) {
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
        }));
    }

    /// Needed to put the requests in a container
    bool operator<(const PendingRequest& other) const
    {
        return (id_ < other.id());
    }


private:
    uint16_t id_;
    boost::asio::steady_timer timer_;
    boost::asio::io_context::strand& strand_;
    MessageHandler<msg::BaseMessage> handler_;
};



/// Endpoint of the server connection
/**
 * Server connection endpoint.
 * Messages are sent to the server with the "send" method (async).
 * Messages are sent sync to server with the sendReq method.
 */
class ClientConnection
{
public:
    using ResultHandler = std::function<void(const boost::system::error_code&)>;

    /// c'tor
    ClientConnection(boost::asio::io_context& io_context, const ClientSettings::Server& server);
    /// d'tor
    virtual ~ClientConnection();

    /// async connect
    /// @param handler async result handler
    void connect(const ResultHandler& handler);
    /// disconnect the socket
    void disconnect();

    /// async send a message
    /// @param message the message
    /// @param handler the result handler
    void send(const msg::message_ptr& message, const ResultHandler& handler);

    /// Send request to the server and wait for answer
    /// @param message the message
    /// @param timeout the send timeout
    /// @param handler async result handler with the response message or error
    void sendRequest(const msg::message_ptr& message, const chronos::usec& timeout, const MessageHandler<msg::BaseMessage>& handler);

    /// @sa sendRequest with templated response message
    template <typename Message>
    void sendRequest(const msg::message_ptr& message, const chronos::usec& timeout, const MessageHandler<Message>& handler)
    {
        sendRequest(message, timeout, [handler](const boost::system::error_code& ec, std::unique_ptr<msg::BaseMessage> response) {
            if (ec)
                handler(ec, nullptr);
            else if (auto casted_response = msg::message_cast<Message>(std::move(response)))
                handler(ec, std::move(casted_response));
            else
                handler(boost::system::errc::make_error_code(boost::system::errc::bad_message), nullptr);
        });
    }

    std::string getMacAddress();

    /// async get the next message
    /// @param handler the next received message or error
    void getNextMessage(const MessageHandler<msg::BaseMessage>& handler);

protected:
    void sendNext();

    msg::BaseMessage base_message_;
    std::vector<char> buffer_;
    size_t base_msg_size_;

    boost::asio::io_context& io_context_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    std::vector<std::weak_ptr<PendingRequest>> pendingRequests_;
    uint16_t reqId_;
    ClientSettings::Server server_;

    boost::asio::io_context::strand strand_;
    struct PendingMessage
    {
        PendingMessage(const msg::message_ptr& msg, ResultHandler handler) : msg(msg), handler(handler)
        {
        }
        msg::message_ptr msg;
        ResultHandler handler;
    };
    std::deque<PendingMessage> messages_;
};



#endif
