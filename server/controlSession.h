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

#ifndef CONTROL_SESSION_H
#define CONTROL_SESSION_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <asio.hpp>
#include <condition_variable>
#include <set>
#include "message/message.h"
#include "common/queue.h"


using asio::ip::tcp;


class ControlSession;


/// Interface: callback for a received message.
class ControlMessageReceiver
{
public:
	virtual void onMessageReceived(ControlSession* connection, const std::string& message) = 0;
};


/// Endpoint for a connected control client.
/**
 * Endpoint for a connected control client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the ControlMessageReceiver callback
 */
class ControlSession
{
public:
	/// ctor. Received message from the client are passed to MessageReceiver
	ControlSession(ControlMessageReceiver* receiver, std::shared_ptr<tcp::socket> socket);
	~ControlSession();
	void start();
	void stop();

	/// Sends a message to the client (synchronous)
	bool send(const std::string& message) const;

	/// Sends a message to the client (asynchronous)
	void sendAsync(const std::string& message);

	bool active() const
	{
		return active_;
	}

protected:
	void reader();
	void writer();

	std::atomic<bool> active_;
	mutable std::mutex activeMutex_;
	mutable std::mutex socketMutex_;
	std::thread* readerThread_;
	std::thread* writerThread_;
	std::shared_ptr<tcp::socket> socket_;
	ControlMessageReceiver* messageReceiver_;
	Queue<std::string> messages_;
};




#endif




