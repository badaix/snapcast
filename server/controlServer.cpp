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

using namespace std;


ControlServer::ControlServer(const ControlServerSettings& controlServerSettings) : settings_(controlServerSettings), sampleFormat_(controlServerSettings.sampleFormat)
{
	serverSettings_.bufferMs = settings_.bufferMs;
}


ControlServer::~ControlServer()
{
}


void ControlServer::send(const msg::BaseMessage* message)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	for (auto it = sessions_.begin(); it != sessions_.end(); )
	{
		if (!(*it)->active())
		{
			logS(kLogErr) << "Session inactive. Removing\n";
			// don't block: remove ServerSession in a thread
			auto func = [](shared_ptr<ServerSession> s)->void{s->stop();};
			std::thread t(func, *it);
			t.detach();
			sessions_.erase(it++);
		}
		else
			++it;
	}

	std::shared_ptr<const msg::BaseMessage> shared_message(message);
	for (auto s : sessions_)
		s->add(shared_message);
}


void ControlServer::onChunkRead(const PipeReader* pipeReader, const msg::PcmChunk* chunk)
{
//	logO << "onChunkRead " << chunk->duration<chronos::msec>().count() << "ms\n";
	send(chunk);
}


void ControlServer::onResync(const PipeReader* pipeReader, double ms)
{
	logO << "onResync " << ms << "ms\n";
}


void ControlServer::onMessageReceived(ServerSession* connection, const msg::BaseMessage& baseMessage, char* buffer)
{
//	logO << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << ", sent: " << baseMessage.sent.sec << "," << baseMessage.sent.usec << ", recv: " << baseMessage.received.sec << "," << baseMessage.received.usec << "\n";
	if (baseMessage.type == message_type::kRequest)
	{
		msg::Request requestMsg;
		requestMsg.deserialize(baseMessage, buffer);
//		logO << "request: " << requestMsg.request << "\n";
		if (requestMsg.request == kTime)
		{
			msg::Time timeMsg;
			timeMsg.refersTo = requestMsg.id;
			timeMsg.latency = (requestMsg.received.sec - requestMsg.sent.sec) + (requestMsg.received.usec - requestMsg.sent.usec) / 1000000.;
//			logD << "Latency: " << timeMsg.latency << ", refers to: " << timeMsg.refersTo << "\n";
			connection->send(&timeMsg);
		}
		else if (requestMsg.request == kServerSettings)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			serverSettings_.refersTo = requestMsg.id;
			connection->send(&serverSettings_);
		}
		else if (requestMsg.request == kSampleFormat)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			sampleFormat_.refersTo = requestMsg.id;
			connection->send(&sampleFormat_);
		}
		else if (requestMsg.request == kHeader)
		{
			std::unique_lock<std::mutex> mlock(mutex_);
			msg::Header* headerChunk = pipeReader_->getHeader();
			headerChunk->refersTo = requestMsg.id;
			connection->send(headerChunk);
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



void ControlServer::startAccept()
{
	socket_ptr socket = make_shared<tcp::socket>(io_service_);
	acceptor_->async_accept(*socket, bind(&ControlServer::handleAccept, this, socket));
}


void ControlServer::handleAccept(socket_ptr socket)
{
	struct timeval tv;
	tv.tv_sec  = 5;
	tv.tv_usec = 0;
	setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(socket->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	logS(kLogNotice) << "ControlServer::NewConnection: " << socket->remote_endpoint().address().to_string() << endl;
	shared_ptr<ServerSession> session = make_shared<ServerSession>(this, socket);
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		session->setBufferMs(settings_.bufferMs);
		session->start();
		sessions_.insert(session);
	}
	startAccept();
}


void ControlServer::start()
{
	pipeReader_ = new PipeReader(this, settings_.sampleFormat, settings_.codec, settings_.fifoName);
	pipeReader_->start();
	acceptor_ = make_shared<tcp::acceptor>(io_service_, tcp::endpoint(tcp::v4(), settings_.port));
	startAccept();
	acceptThread_ = thread(&ControlServer::acceptor, this);
}


void ControlServer::stop()
{
	acceptor_->cancel();
	io_service_.stop();
	acceptThread_.join();
	pipeReader_->stop();
	std::unique_lock<std::mutex> mlock(mutex_);
	for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
		(*it)->stop();
}


void ControlServer::acceptor()
{
	io_service_.run();
}

