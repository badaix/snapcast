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

#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

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

class MessageReceiver
{
public:
	virtual void onMessageReceived(ServerSession* connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
};


class ServerSession
{
public:
	ServerSession(MessageReceiver* receiver, std::shared_ptr<tcp::socket> socket);
	~ServerSession();
	void start();
	void stop();
	bool send(const msg::BaseMessage* message) const;
	void add(const std::shared_ptr<const msg::BaseMessage>& message);

	virtual bool active() const
	{
		return active_;
	}

	virtual void setStreamActive(bool active)
	{
		streamActive_ = active;
	}


protected:
	void socketRead(void* _to, size_t _bytes);
	void getNextMessage();
	void reader();
	void writer();

	std::atomic<bool> active_;
	std::atomic<bool> streamActive_;
	mutable std::mutex mutex_;
	std::thread* readerThread_;
	std::thread* writerThread_;
	std::shared_ptr<tcp::socket> socket_;
	MessageReceiver* messageReceiver_;
	Queue<std::shared_ptr<const msg::BaseMessage>> messages_;
};




#endif




