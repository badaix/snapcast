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

#include "streamSession.h"

#include <iostream>
#include <mutex>
#include "common/log.h"
#include "message/pcmChunk.h"

using namespace std;



StreamSession::StreamSession(MessageReceiver* receiver, std::shared_ptr<tcp::socket> socket) :
	active_(true), messageReceiver_(receiver), pcmStream_(nullptr)
{
	socket_ = socket;
}


StreamSession::~StreamSession()
{
	stop();
}


void StreamSession::setPcmStream(PcmStreamPtr pcmStream)
{
	pcmStream_ = pcmStream;
}


const PcmStreamPtr StreamSession::pcmStream() const
{
	return pcmStream_;
}


void StreamSession::start()
{
	setActive(true);
	readerThread_ = new thread(&StreamSession::reader, this);
	writerThread_ = new thread(&StreamSession::writer, this);
}


void StreamSession::stop()
{
	setActive(false);
	std::unique_lock<std::mutex> mlock(mutex_);
	try
	{
		std::error_code ec;
		if (socket_)
		{
			socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) logE << "Error in socket shutdown: " << ec.message() << "\n";
			socket_->close(ec);
			if (ec) logE << "Error in socket close: " << ec.message() << "\n";
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
	socket_ = NULL;
	logD << "StreamSession stopped\n";
}


void StreamSession::socketRead(void* _to, size_t _bytes)
{
	size_t read = 0;
	do
	{
		read += socket_->read_some(asio::buffer((char*)_to + read, _bytes - read));
	}
	while (active_ && (read < _bytes));
}


void StreamSession::add(const shared_ptr<const msg::BaseMessage>& message)
{
	if (!message)
		return;

	while (messages_.size() > 100)// chunk->getDuration() > 10000)
		messages_.pop();
	messages_.push(message);
}


bool StreamSession::active() const
{
	return active_;
}


void StreamSession::setBufferMs(size_t bufferMs)
{
	bufferMs_ = bufferMs;
}


bool StreamSession::send(const msg::BaseMessage* message) const
{
	//TODO on exception: set active = false
//	logO << "send: " << message->type << ", size: " << message->getSize() << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
	std::unique_lock<std::mutex> mlock(mutex_);
	if (!socket_ || !active_)
		return false;
	asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	tv t;
	message->sent = t;
	message->serialize(stream);
	asio::write(*socket_.get(), streambuf);
//	logO << "done: " << message->type << ", size: " << message->size << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
	return true;
}


void StreamSession::getNextMessage()
{
	msg::BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
	if (baseMessage.size > msg::max_size)
	{
		logS(kLogErr) << "received message of type " << baseMessage.type << " to large: " << baseMessage.size << "\n";
		stop();
		return;
	}
//	logO << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(&buffer[0], baseMessage.size);
	tv t;
	baseMessage.received = t;

	if (messageReceiver_ != NULL)
		messageReceiver_->onMessageReceived(this, baseMessage, &buffer[0]);
}


void StreamSession::reader()
{
	try
	{
		while (active_)
		{
			getNextMessage();
		}
	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception in StreamSession::reader(): " << e.what() << endl;
	}
	setActive(false);
}


void StreamSession::writer()
{
	try
	{
		asio::streambuf streambuf;
		std::ostream stream(&streambuf);
		shared_ptr<const msg::BaseMessage> message;
		while (active_)
		{
			if (messages_.try_pop(message, std::chrono::milliseconds(500)))
			{
				if (bufferMs_ > 0)
				{
					const msg::WireChunk* wireChunk = dynamic_cast<const msg::WireChunk*>(message.get());
					if (wireChunk != NULL)
					{
						chronos::time_point_clk now = chronos::clk::now();
						size_t age = 0;
						if (now > wireChunk->start())
							age = std::chrono::duration_cast<chronos::msec>(now - wireChunk->start()).count();
						//logD << "PCM chunk. Age: " << age << ", buffer: " << bufferMs_ << ", age > buffer: " << (age > bufferMs_) << "\n";
						if (age > bufferMs_)
							continue;
					}
				}
				send(message.get());
			}
		}
	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception in StreamSession::writer(): " << e.what() << endl;
	}
	setActive(false);
}


void StreamSession::setActive(bool active)
{
	std::lock_guard<std::mutex> mlock(activeMutex_);
	if (active_ && !active && (messageReceiver_ != NULL))
		messageReceiver_->onDisconnect(this);
	active_ = active;
}


