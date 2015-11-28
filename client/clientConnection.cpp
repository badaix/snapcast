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
#include "common/log.h"
#include "clientConnection.h"
#include "common/utils.h"
#include "common/snapException.h"


using namespace std;


ClientConnection::ClientConnection(MessageReceiver* receiver, const std::string& ip, size_t port) : active_(false), connected_(false), messageReceiver_(receiver), reqId_(1), ip_(ip), port_(port), readerThread_(NULL), sumTimeout_(chronos::msec(0))
{
}


ClientConnection::~ClientConnection()
{
}



void ClientConnection::socketRead(void* _to, size_t _bytes)
{
//	std::unique_lock<std::mutex> mlock(mutex_);
	size_t toRead = _bytes;
	size_t len = 0;
	do
	{
//		boost::system::error_code error;
		len += socket_->read_some(boost::asio::buffer((char*)_to + len, toRead));
//cout << "len: " << len << ", error: " << error << endl;
		toRead = _bytes - len;
	}
	while (toRead > 0);
}


void ClientConnection::start()
{
	tcp::resolver resolver(io_service_);
	tcp::resolver::query query(tcp::v4(), ip_, boost::lexical_cast<string>(port_), boost::asio::ip::resolver_query_base::numeric_service);
	auto iterator = resolver.resolve(query);
	logO << "Connecting\n";
	socket_.reset(new tcp::socket(io_service_));
//	struct timeval tv;
//	tv.tv_sec  = 5;
//	tv.tv_usec = 0;
//	cout << "socket: " << socket->native() << "\n";
//	setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//	setsockopt(socket->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	socket_->connect(*iterator);
	logO << "My MAC: \"" << getMacAddress(socket_->native()) << "\"\n";
	connected_ = true;
	logS(kLogNotice) << "Connected to " << socket_->remote_endpoint().address().to_string() << endl;
	active_ = true;
	readerThread_ = new thread(&ClientConnection::reader, this);
}


void ClientConnection::stop()
{
	connected_ = false;
	active_ = false;
	try
	{
		boost::system::error_code ec;
		if (socket_)
		{
			socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) logE << "Error in socket shutdown: " << ec << endl;
			socket_->close(ec);
			if (ec) logE << "Error in socket close: " << ec << endl;
		}
		if (readerThread_ && readerThread_->joinable())
		{
			logD << "joining readerThread\n";
			readerThread_->join();
			delete readerThread_;
		}
	}
	catch(...)
	{
	}
	readerThread_ = NULL;
	logD << "readerThread terminated\n";
}


bool ClientConnection::send(const msg::BaseMessage* message) const
{
//	std::unique_lock<std::mutex> mlock(mutex_);
//logD << "send: " << message->type << ", size: " << message->getSize() << "\n";
	if (!connected())
		return false;
//logD << "send: " << message->type << ", size: " << message->getSize() << "\n";
	boost::asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	tv t;
	message->sent = t;
	message->serialize(stream);
	boost::asio::write(*socket_.get(), streambuf);
	return true;
}


shared_ptr<msg::SerializedMessage> ClientConnection::sendRequest(const msg::BaseMessage* message, const chronos::msec& timeout)
{
	shared_ptr<msg::SerializedMessage> response(NULL);
	if (++reqId_ >= 10000)
		reqId_ = 1;
	message->id = reqId_;
//	logO << "Req: " << message->id << "\n";
	shared_ptr<PendingRequest> pendingRequest(new PendingRequest(reqId_));

	{
		std::unique_lock<std::mutex> mlock(mutex_);
		pendingRequests_.insert(pendingRequest);
	}
	std::unique_lock<std::mutex> lck(requestMutex_);
	send(message);
	if (pendingRequest->cv.wait_for(lck, std::chrono::milliseconds(timeout)) == std::cv_status::no_timeout)
	{
		response = pendingRequest->response;
		sumTimeout_ = chronos::msec(0);
//		logO << "Resp: " << pendingRequest->id << "\n";
	}
	else
	{
		sumTimeout_ += timeout;
		logO << "timeout while waiting for response to: " << reqId_ << ", timeout " << sumTimeout_.count() << "\n";
		if (sumTimeout_ > chronos::sec(10))
			throw SnapException("sum timeout exceeded 10s");
	}
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		pendingRequests_.erase(pendingRequest);
	}
	return response;
}


void ClientConnection::getNextMessage()
{
	msg::BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
//	logD << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(&buffer[0], baseMessage.size);
	tv t;
	baseMessage.received = t;

	{
		std::unique_lock<std::mutex> mlock(mutex_);
//		logD << "got lock - getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
		{
			for (auto req: pendingRequests_)
			{
				if (req->id == baseMessage.refersTo)
				{
					req->response.reset(new msg::SerializedMessage());
					req->response->message = baseMessage;
					req->response->buffer = (char*)malloc(baseMessage.size);
					memcpy(req->response->buffer, &buffer[0], baseMessage.size);
					std::unique_lock<std::mutex> lck(requestMutex_);
					req->cv.notify_one();
					return;
				}
			}
		}
	}

	if (messageReceiver_ != NULL)
		messageReceiver_->onMessageReceived(this, baseMessage, &buffer[0]);
}



void ClientConnection::reader()
{
	try
	{
		while(active_)
		{
			getNextMessage();
		}
	}
	catch (const std::exception& e)
	{
		if (messageReceiver_ != NULL)
			messageReceiver_->onException(this, e);
	}
	catch (...)
	{
	}
	connected_ = false;
	active_ = false;
}




