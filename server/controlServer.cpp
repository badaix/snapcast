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

#include "controlServer.h"
#include "message/time.h"
#include "message/ack.h"
#include "message/request.h"
#include "message/command.h"
#include "common/log.h"
#include <iostream>


ControlServer::ControlServer(unsigned short port) : port_(port), headerChunk_(NULL), sampleFormat_(NULL)
{
}




void ControlServer::send(shared_ptr<msg::BaseMessage> message)
{
	std::unique_lock<std::mutex> mlock(mutex);
	for (auto it = sessions_.begin(); it != sessions_.end(); )
	{
		if (!(*it)->active())
		{
			logO << "Session inactive. Removing\n";
			auto func = [](shared_ptr<ServerSession> s)->void{s->stop();};
			std::thread t(func, *it);
			t.detach();
			sessions_.erase(it++);
		}
		else
			++it;
	}

	for (auto s : sessions_)
		s->add(message);
}


void ControlServer::onMessageReceived(ServerSession* connection, const msg::BaseMessage& baseMessage, char* buffer)
{
//	logD << "onMessageReceived: " << baseMessage.type << ", size: " << baseMessage.size << ", sent: " << baseMessage.sent.sec << "," << baseMessage.sent.usec << ", recv: " << baseMessage.received.sec << "," << baseMessage.received.usec << "\n";
	if (baseMessage.type == message_type::kRequest)
	{
		msg::Request requestMsg;
		requestMsg.deserialize(baseMessage, buffer);
//		logD << "request: " << requestMsg.request << "\n";
		if (requestMsg.request == kTime)
		{
//		timeMsg.latency = (timeMsg.received.sec - timeMsg.sent.sec) * 1000000 + (timeMsg.received.usec - timeMsg.sent.usec);
			msg::Time timeMsg;
			timeMsg.refersTo = requestMsg.id;
			timeMsg.latency = (requestMsg.received.sec - requestMsg.sent.sec) + (requestMsg.received.usec - requestMsg.sent.usec) / 1000000.;
//		tv diff = timeMsg.received - timeMsg.sent;
//		logD << "Latency: " << diff.sec << "." << diff.usec << "\n";
			connection->send(&timeMsg);
		}
		else if (requestMsg.request == kServerSettings)
		{
			serverSettings_->refersTo = requestMsg.id;
			connection->send(serverSettings_);
		}
		else if (requestMsg.request == kSampleFormat)
		{
			sampleFormat_->refersTo = requestMsg.id;
			connection->send(sampleFormat_);
		}
		else if (requestMsg.request == kHeader)
		{
			headerChunk_->refersTo = requestMsg.id;
			connection->send(headerChunk_);
		}
	}
	else if (baseMessage.type == message_type::kCommand)
	{
		msg::Command commandMsg;
		commandMsg.deserialize(baseMessage, buffer);
		if (commandMsg.command == "startStream")
		{
			msg::Ack ackMsg;
			ackMsg.refersTo = commandMsg.id;
			connection->send(&ackMsg);
			connection->setStreamActive(true);
		}
	}
}


void ControlServer::acceptor()
{
	tcp::acceptor a(io_service_, tcp::endpoint(tcp::v4(), port_));
	for (;;)
	{
		socket_ptr sock(new tcp::socket(io_service_));
		struct timeval tv;
		tv.tv_sec  = 5;
		tv.tv_usec = 0;
		a.accept(*sock);
		setsockopt(sock->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(sock->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
		logS(kLogNotice) << "ControlServer::NewConnection: " << sock->remote_endpoint().address().to_string() << endl;
		ServerSession* session = new ServerSession(this, sock);
		{
			std::unique_lock<std::mutex> mlock(mutex);
			session->start();
			sessions_.insert(shared_ptr<ServerSession>(session));
		}
	}
}


void ControlServer::start()
{
	acceptThread_ = new thread(&ControlServer::acceptor, this);
}


void ControlServer::stop()
{
//		acceptThread->join();
}


void ControlServer::setHeader(msg::Header* header)
{
	if (header)
		headerChunk_ = header;
}


void ControlServer::setFormat(msg::SampleFormat* format)
{
	if (format)
		sampleFormat_ = format;
}



void ControlServer::setServerSettings(msg::ServerSettings* settings)
{
	if (settings)
		serverSettings_ = settings;
}





