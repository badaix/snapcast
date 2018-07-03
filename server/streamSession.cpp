/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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
#include "aixlog.hpp"
#include "message/pcmChunk.h"

using namespace std;



StreamSession::StreamSession(MessageReceiver* receiver, std::shared_ptr<tcp::socket> socket) :
	active_(false), readerThread_(nullptr), writerThread_(nullptr), messageReceiver_(receiver), pcmStream_(nullptr)
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
	{
		std::lock_guard<std::mutex> activeLock(activeMutex_);
		active_ = true;
	}
	readerThread_.reset(new thread(&StreamSession::reader, this));
	writerThread_.reset(new thread(&StreamSession::writer, this));
}


void StreamSession::stop()
{
	{
		std::lock_guard<std::mutex> activeLock(activeMutex_);
		if (!active_)
			return;

		active_ = false;
	}

	try
	{
		std::error_code ec;
		if (socket_)
		{
			std::lock_guard<std::mutex> socketLock(socketMutex_);
			socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) LOG(ERROR) << "Error in socket shutdown: " << ec.message() << "\n";
			socket_->close(ec);
			if (ec) LOG(ERROR) << "Error in socket close: " << ec.message() << "\n";
		}
		if (readerThread_ && readerThread_->joinable())
		{
			LOG(DEBUG) << "StreamSession joining readerThread\n";
			readerThread_->join();
		}
		if (writerThread_ && writerThread_->joinable())
		{
			LOG(DEBUG) << "StreamSession joining writerThread\n";
			messages_.abort_wait();
			writerThread_->join();
		}
	}
	catch(...)
	{
	}

	readerThread_ = nullptr;
	writerThread_ = nullptr;
	socket_ = nullptr;
	LOG(DEBUG) << "StreamSession stopped\n";
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


void StreamSession::sendAsync(const msg::message_ptr& message, bool sendNow)
{
	if (!message)
		return;

	//the writer will take care about old messages
	while (messages_.size() > 2000)// chunk->getDuration() > 10000)
		messages_.pop();

	if (sendNow)
		messages_.push_front(message);
	else	
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


bool StreamSession::send(const msg::message_ptr& message) const
{
	//TODO on exception: set active = false
//	LOG(INFO) << "send: " << message->type << ", size: " << message->getSize() << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
	std::lock_guard<std::mutex> socketLock(socketMutex_);
	{
		std::lock_guard<std::mutex> activeLock(activeMutex_);
		if (!socket_ || !active_)
			return false;
	}
	asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	tv t;
	message->sent = t;
	message->serialize(stream);
	asio::write(*socket_.get(), streambuf);
//	LOG(INFO) << "done: " << message->type << ", size: " << message->size << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
	return true;
}


void StreamSession::getNextMessage()
{
	msg::BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);

	if ((baseMessage.type > message_type::kLast) || (baseMessage.type < message_type::kFirst))
	{
		stringstream ss;
		ss << "unknown message type received: " << baseMessage.type << ", size: " << baseMessage.size;
		throw std::runtime_error(ss.str().c_str());
	}
	else if (baseMessage.size > msg::max_size)
	{
		stringstream ss;
		ss << "received message of type " << baseMessage.type << " to large: " << baseMessage.size;
		throw std::runtime_error(ss.str().c_str());
	}
	
//	LOG(INFO) << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
//	{
//		std::lock_guard<std::mutex> socketLock(socketMutex_);
	socketRead(&buffer[0], baseMessage.size);
//	}	
	tv t;
	baseMessage.received = t;

	if (active_ && (messageReceiver_ != NULL))
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
		SLOG(ERROR) << "Exception in StreamSession::reader(): " << e.what() << endl;
	}

	if (active_ && (messageReceiver_ != NULL))
		messageReceiver_->onDisconnect(this);
}


void StreamSession::writer()
{
	try
	{
		asio::streambuf streambuf;
		std::ostream stream(&streambuf);
		shared_ptr<msg::BaseMessage> message;
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
						//LOG(DEBUG) << "PCM chunk. Age: " << age << ", buffer: " << bufferMs_ << ", age > buffer: " << (age > bufferMs_) << "\n";
						if (age > bufferMs_)
							continue;
					}
				}
				send(message);
			}
		}
	}
	catch (const std::exception& e)
	{
		SLOG(ERROR) << "Exception in StreamSession::writer(): " << e.what() << endl;
	}

	if (active_ && (messageReceiver_ != NULL))
		messageReceiver_->onDisconnect(this);
}


