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

#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

// local headers
#include "client_settings.hpp"
#include "common/message/factory.hpp"
#include "common/message/message.hpp"
#include "common/time_defs.hpp"

// 3rd party headers
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

// standard headers
#include <atomic>
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
    PendingRequest(const boost::asio::strand<boost::asio::any_io_executor>& strand, uint16_t reqId, const MessageHandler<msg::BaseMessage>& handler);
    virtual ~PendingRequest();

    /// Set the response for the pending request and passes it to the handler
    /// @param value the response message
    void setValue(std::unique_ptr<msg::BaseMessage> value);

    /// @return the id of the request
    uint16_t id() const;

    /// Start the timer for the request
    /// @param timeout the timeout to wait for the reception of the response
    void startTimer(const chronos::usec& timeout);

    /// Needed to put the requests in a container
    bool operator<(const PendingRequest& other) const;

private:
    uint16_t id_;
    boost::asio::steady_timer timer_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
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
        sendRequest(message, timeout,
                    [handler](const boost::system::error_code& ec, std::unique_ptr<msg::BaseMessage> response)
                    {
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

    boost::asio::strand<boost::asio::any_io_executor> strand_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    std::vector<std::weak_ptr<PendingRequest>> pendingRequests_;
    uint16_t reqId_;
    ClientSettings::Server server_;

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
