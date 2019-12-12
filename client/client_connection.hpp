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

#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include "common/time_defs.hpp"
#include "message/message.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>


using boost::asio::ip::tcp;


class ClientConnection;


/// Used to synchronize server requests (wait for server response)
class PendingRequest
{
public:
    PendingRequest(uint16_t reqId) : id_(reqId)
    {
        future_ = promise_.get_future();
    };

    template <typename Rep, typename Period>
    std::unique_ptr<msg::BaseMessage> waitForResponse(const std::chrono::duration<Rep, Period>& timeout)
    {
        try
        {
            if (future_.wait_for(timeout) == std::future_status::ready)
                return future_.get();
        }
        catch (...)
        {
        }
        return nullptr;
    }

    void setValue(std::unique_ptr<msg::BaseMessage> value)
    {
        promise_.set_value(std::move(value));
    }

    uint16_t id() const
    {
        return id_;
    }

private:
    uint16_t id_;

    std::promise<std::unique_ptr<msg::BaseMessage>> promise_;
    std::future<std::unique_ptr<msg::BaseMessage>> future_;
};


/// would be nicer to use std::exception_ptr
/// but not supported on all plattforms
typedef std::shared_ptr<std::exception> shared_exception_ptr;

/// Interface: callback for a received message and error reporting
class MessageReceiver
{
public:
    virtual ~MessageReceiver() = default;
    virtual void onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
    virtual void onException(ClientConnection* connection, shared_exception_ptr exception) = 0;
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
    /// ctor. Received message from the server are passed to MessageReceiver
    ClientConnection(MessageReceiver* receiver, const std::string& host, size_t port);
    virtual ~ClientConnection();
    virtual void start();
    virtual void stop();
    virtual bool send(const msg::BaseMessage* message);

    /// Send request to the server and wait for answer
    virtual std::unique_ptr<msg::BaseMessage> sendRequest(const msg::BaseMessage* message, const chronos::msec& timeout = chronos::msec(1000));

    /// Send request to the server and wait for answer of type T
    template <typename T>
    std::unique_ptr<T> sendReq(const msg::BaseMessage* message, const chronos::msec& timeout = chronos::msec(1000))
    {
        std::unique_ptr<msg::BaseMessage> response = sendRequest(message, timeout);
        if (!response)
            return nullptr;

        T* tmp = dynamic_cast<T*>(response.get());
        std::unique_ptr<T> result;
        if (tmp != nullptr)
        {
            response.release();
            result.reset(tmp);
        }
        return result;
    }

    std::string getMacAddress();

    virtual bool active() const
    {
        return active_;
    }

protected:
    virtual void reader();

    void socketRead(void* to, size_t bytes);
    void getNextMessage();

    msg::BaseMessage base_message_;
    std::vector<char> buffer_;
    size_t base_msg_size_;

    boost::asio::io_context io_context_;
    mutable std::mutex socketMutex_;
    tcp::socket socket_;
    std::atomic<bool> active_;
    MessageReceiver* messageReceiver_;
    mutable std::mutex pendingRequestsMutex_;
    std::set<std::shared_ptr<PendingRequest>> pendingRequests_;
    uint16_t reqId_;
    std::string host_;
    size_t port_;
    std::thread* readerThread_;
    chronos::msec sumTimeout_;
};



#endif
