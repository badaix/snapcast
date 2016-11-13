/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <asio.hpp>
#include <condition_variable>
#include <set>
#include "message/message.h"
#include "common/timeDefs.h"


using asio::ip::tcp;


class ClientConnection;


/// Used to synchronize server requests (wait for server response)
struct PendingRequest
{
	PendingRequest(uint16_t reqId) : id(reqId), response(NULL) {};

	uint16_t id;
	std::shared_ptr<msg::SerializedMessage> response;
	std::condition_variable cv;
};


/// Interface: callback for a received message and error reporting
class MessageReceiver
{
public:
	virtual void onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
	virtual void onException(ClientConnection* connection, const std::exception& exception) = 0;
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
	virtual bool send(const msg::BaseMessage* message) const;

	/// Send request to the server and wait for answer
	virtual std::shared_ptr<msg::SerializedMessage> sendRequest(const msg::BaseMessage* message, const chronos::msec& timeout = chronos::msec(1000));

	/// Send request to the server and wait for answer of type T
	template <typename T>
	std::shared_ptr<T> sendReq(const msg::BaseMessage* message, const chronos::msec& timeout = chronos::msec(1000))
	{
		std::shared_ptr<msg::SerializedMessage> reply = sendRequest(message, timeout);
		if (!reply)
			return NULL;
		std::shared_ptr<T> msg(new T);
		msg->deserialize(reply->message, reply->buffer);
		return msg;
	}

	std::string getMacAddress() const;

	virtual bool active() const
	{
		return active_;
	}

	virtual bool connected() const
	{
		return (socket_ != nullptr);
//		return (connected_ && socket);
	}

protected:
	virtual void reader();

	void socketRead(void* to, size_t bytes);
	void getNextMessage();

	asio::io_service io_service_;
	mutable std::mutex socketMutex_;
	std::shared_ptr<tcp::socket> socket_;
	std::atomic<bool> active_;
	std::atomic<bool> connected_;
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




