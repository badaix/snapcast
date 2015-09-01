/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#ifndef SERVER_SESSION_H
#define SERVER_SESSION_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <boost/asio.hpp>
#include <condition_variable>
#include <set>
#include "message/message.h"
#include "common/queue.h"


using boost::asio::ip::tcp;


class ServerSession;


/// Interface: callback for a received message.
class MessageReceiver
{
public:
	virtual void onMessageReceived(ServerSession* connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
	virtual void onDisconnect(ServerSession* connection) = 0;
};


/// Endpoint for a connected client.
/**
 * Endpoint for a connected client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the MessageReceiver callback
 */
class ServerSession
{
public:
	/// ctor. Received message from the client are passed to MessageReceiver
	ServerSession(MessageReceiver* receiver, std::shared_ptr<tcp::socket> socket);
	~ServerSession();
	void start();
	void stop();

	/// Sends a message to the client (synchronous)
	bool send(const msg::BaseMessage* message) const;

	/// Sends a message to the client (asynchronous)
	void add(const std::shared_ptr<const msg::BaseMessage>& message);

	bool active() const
	{
		return active_;
	}

	/// Client subscribed for the PCM stream, by sending the "startStream" command
	/// TODO: Currently there is only one stream ("zone")
	void setStreamActive(bool active)
	{
		streamActive_ = active;
	}

	/// Max playout latency. No need to send PCM data that is older than bufferMs
	void setBufferMs(size_t bufferMs)
	{
		bufferMs_ = bufferMs;
	}

	std::string macAddress;

	std::string getIP()
	{
		return socket_->remote_endpoint().address().to_string();
	}

protected:
	void socketRead(void* _to, size_t _bytes);
	void getNextMessage();
	void reader();
	void writer();
	void setActive(bool active);

	std::atomic<bool> active_;
	std::atomic<bool> streamActive_;
	mutable std::mutex mutex_;
	std::thread* readerThread_;
	std::thread* writerThread_;
	std::shared_ptr<tcp::socket> socket_;
	MessageReceiver* messageReceiver_;
	Queue<std::shared_ptr<const msg::BaseMessage>> messages_;
	size_t bufferMs_;
};




#endif




