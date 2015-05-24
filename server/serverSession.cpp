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

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <mutex>
#include "serverSession.h"
#include "common/log.h"

using namespace std;




ServerSession::ServerSession(MessageReceiver* receiver, std::shared_ptr<tcp::socket> socket) : messageReceiver_(receiver)
{
	socket_ = socket;
}


ServerSession::~ServerSession()
{
	stop();
}


void ServerSession::start()
{
	active_ = true;
	streamActive_ = false;
	readerThread_ = new thread(&ServerSession::reader, this);
	writerThread_ = new thread(&ServerSession::writer, this);
}


void ServerSession::stop()
{
	active_ = false;
	try
	{
		boost::system::error_code ec;
		if (socket_)
		{
			socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) logE << "Error in socket shutdown: " << ec << "\n";
			socket_->close(ec);
			if (ec) logE << "Error in socket close: " << ec << "\n";
		}
		if (readerThread_)
		{
			logD << "joining readerThread\n";
			readerThread_->join();
			delete readerThread_;
		}
		if (writerThread_)
		{
			logD << "joining writerThread\n";
			writerThread_->join();
			delete writerThread_;
		}
	}
	catch(...)
	{
	}
	readerThread_ = NULL;
	writerThread_ = NULL;
	logD << "ServerSession stopped\n";
}



void ServerSession::socketRead(void* _to, size_t _bytes)
{
	size_t read = 0;
	do
	{
		boost::system::error_code error;
		read += socket_->read_some(boost::asio::buffer((char*)_to + read, _bytes - read));
	}
	while (read < _bytes);
}


void ServerSession::add(shared_ptr<msg::BaseMessage> message)
{
	if (!message || !streamActive_)
		return;

	while (messages_.size() > 100)// chunk->getDuration() > 10000)
		messages_.pop();
	messages_.push(message);
}


bool ServerSession::send(msg::BaseMessage* message)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	if (!socket_)
		return false;
	boost::asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	tv t;
	message->sent = t;
	message->serialize(stream);
	boost::asio::write(*socket_.get(), streambuf);
	return true;
}


void ServerSession::getNextMessage()
{
//logD << "getNextMessage\n";
	msg::BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
//logD << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(&buffer[0], baseMessage.size);
	tv t;
	baseMessage.received = t;

	if (messageReceiver_ != NULL)
		messageReceiver_->onMessageReceived(this, baseMessage, &buffer[0]);
}



void ServerSession::reader()
{
	active_ = true;
	try
	{
		while (active_)
		{
			getNextMessage();
		}
	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception in ServerSession::reader(): " << e.what() << endl;
	}
	active_ = false;
}




void ServerSession::writer()
{
	try
	{
		boost::asio::streambuf streambuf;
		std::ostream stream(&streambuf);
		shared_ptr<msg::BaseMessage> message;
		while (active_)
		{
			if (messages_.try_pop(message, std::chrono::milliseconds(500)))
				send(message.get());
		}
	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception in ServerSession::writer(): " << e.what() << endl;
	}
	active_ = false;
}










