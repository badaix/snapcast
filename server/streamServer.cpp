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

#include "json/jsonrpc.h"
#include "streamServer.h"
#include "message/time.h"
#include "message/ack.h"
#include "message/request.h"
#include "message/command.h"
#include "message/hello.h"
#include "common/log.h"
#include "config.h"
#include <iostream>

using namespace std;

using json = nlohmann::json;


StreamServer::StreamServer(asio::io_service* io_service, const StreamServerSettings& streamServerSettings) : io_service_(io_service), settings_(streamServerSettings), sampleFormat_(streamServerSettings.sampleFormat)
{
}


StreamServer::~StreamServer()
{
}


void StreamServer::send(const msg::BaseMessage* message)
{
	std::unique_lock<std::mutex> mlock(mutex_);


	for (auto it = sessions_.begin(); it != sessions_.end(); )
	{
		if (!(*it)->active())
		{
			logS(kLogErr) << "Session inactive. Removing\n";
			// don't block: remove ServerSession in a thread
			onDisconnect(it->get());
			auto func = [](shared_ptr<StreamSession> s)->void{s->stop();};
			std::thread t(func, *it);
			t.detach();
			sessions_.erase(it++);
		}
		else
			++it;
	}


/*	for (auto it = sessions_.begin(); it != sessions_.end(); )
	{
		if (!(*it)->active())
			onDisconnect(it->get());
	}
*/
	std::shared_ptr<const msg::BaseMessage> shared_message(message);
	for (auto s : sessions_)
		s->add(shared_message);
}


void StreamServer::onChunkRead(const PcmReader* pcmReader, const msg::PcmChunk* chunk, double duration)
{
//	logO << "onChunkRead " << duration << "ms\n";
	send(chunk);
}


void StreamServer::onResync(const PcmReader* pcmReader, double ms)
{
	logO << "onResync " << ms << "ms\n";
}


void StreamServer::onDisconnect(StreamSession* streamSession)
{
	logO << "onDisconnect: " << streamSession->macAddress << "\n";
/*	auto func = [](StreamSession* s)->void{s->stop();};
	std::thread t(func, streamSession);
	t.detach();
*/
	ClientInfoPtr clientInfo = Config::instance().getClientInfo(streamSession->macAddress);
/*	// don't block: remove StreamSession in a thread
	for (auto it = sessions_.begin(); it != sessions_.end(); )
	{
		if (it->get() == streamSession)
		{
	logO << "erase: " << (*it)->macAddress << "\n";
			sessions_.erase(it);
			break;
		}
	}
*/
	// notify controllers if not yet done
	if (!clientInfo->connected)
		return;
	clientInfo->connected = false;
	gettimeofday(&clientInfo->lastSeen, NULL);
	Config::instance().save();
	json notification = JsonNotification::getJson("Client.OnDisconnect", clientInfo->toJson());
	controlServer_->send(notification.dump());
}


void StreamServer::onMessageReceived(ControlSession* controlSession, const std::string& message)
{
	JsonRequest request;
	try
	{
		request.parse(message);
		logO << "method: " << request.method << ", " << "id: " << request.id << "\n";

		json response;
		ClientInfoPtr clientInfo = nullptr;
		msg::ServerSettings serverSettings;
		serverSettings.bufferMs = settings_.bufferMs;

		if (request.method.find("Client.Set") == 0)
		{
			clientInfo = Config::instance().getClientInfo(request.getParam("client").get<string>(), false);
			if (clientInfo == nullptr)
				throw JsonInternalErrorException("Client not found", request.id);
		}

		if (request.method == "System.GetStatus")
		{
			json jClient = json::array();
			if (request.hasParam("client"))
			{
				ClientInfoPtr client = Config::instance().getClientInfo(request.getParam("client").get<string>(), false);
				if (client)
					jClient += client->toJson();
			}
			else
				jClient = Config::instance().getClientInfos();

			response = {
				{"server", {
					{"host", getHostName()},
					{"version", VERSION}
				}},
				{"clients", jClient}
			};
		}
		else if (request.method == "Client.SetVolume")
		{
			clientInfo->volume.percent = request.getParam<uint16_t>("volume", 0, 100);
			response = clientInfo->volume.percent;
		}
		else if (request.method == "Client.SetMute")
		{
			clientInfo->volume.muted = request.getParam<bool>("mute", false, true);
			response = clientInfo->volume.muted;
		}
		else if (request.method == "Client.SetLatency")
		{
			clientInfo->latency = request.getParam<int>("latency", -10000, settings_.bufferMs);
			response = clientInfo->latency;
		}
		else if (request.method == "Client.SetName")
		{
			clientInfo->name = request.getParam("name").get<string>();
			response = clientInfo->name;
		}
		else
			throw JsonMethodNotFoundException(request.id);

		if (clientInfo != nullptr)
		{
			serverSettings.volume = clientInfo->volume.percent;
			serverSettings.muted = clientInfo->volume.muted;
			serverSettings.latency = clientInfo->latency;

			StreamSession* session = getStreamSession(request.getParam("client").get<string>());
			if (session != NULL)
				session->send(&serverSettings);

			Config::instance().save();
			json notification = JsonNotification::getJson("Client.OnUpdate", clientInfo->toJson());
			controlServer_->send(notification.dump());
		}

		controlSession->send(request.getResponse(response).dump());
	}
	catch (const JsonRequestException& e)
	{
		controlSession->send(e.getResponse().dump());
	}
	catch (const exception& e)
	{
		JsonInternalErrorException jsonException(e.what(), request.id);
		controlSession->send(jsonException.getResponse().dump());
	}
}


void StreamServer::onMessageReceived(StreamSession* connection, const msg::BaseMessage& baseMessage, char* buffer)
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
			ClientInfoPtr clientInfo = Config::instance().getClientInfo(connection->macAddress, true);
			msg::ServerSettings serverSettings;
			serverSettings.volume = clientInfo->volume.percent;
			serverSettings.muted = clientInfo->volume.muted;
			serverSettings.latency = clientInfo->latency;
			serverSettings.refersTo = requestMsg.id;
			serverSettings.bufferMs = settings_.bufferMs;
			connection->send(&serverSettings);
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
			msg::Header* headerChunk = pcmReader_->getHeader();
			headerChunk->refersTo = requestMsg.id;
			connection->send(headerChunk);
		}
	}
	else if (baseMessage.type == message_type::kCommand)
	{
		msg::Command commandMsg;
		commandMsg.deserialize(baseMessage, buffer);
		if (commandMsg.getCommand() == "startStream")
		{
			msg::Ack ackMsg;
			ackMsg.refersTo = commandMsg.id;
			connection->send(&ackMsg);
			connection->setStreamActive(true);
		}
	}
	else if (baseMessage.type == message_type::kHello)
	{
		msg::Hello helloMsg;
		helloMsg.deserialize(baseMessage, buffer);
		connection->macAddress = helloMsg.getMacAddress();
		logO << "Hello from " << connection->macAddress << ", host: " << helloMsg.getHostName() << ", v" << helloMsg.getVersion() << "\n";

		ClientInfoPtr client = Config::instance().getClientInfo(connection->macAddress);
		client->ipAddress = connection->getIP();
		client->hostName = helloMsg.getHostName();
		client->version = helloMsg.getVersion();
		client->connected = true;
		gettimeofday(&client->lastSeen, NULL);
		Config::instance().save();

		json notification = JsonNotification::getJson("Client.OnConnect", client->toJson());
		controlServer_->send(notification.dump());
	}
}


StreamSession* StreamServer::getStreamSession(const std::string& mac)
{
	for (auto session: sessions_)
	{
		if (session->macAddress == mac)
			return session.get();
	}
	return NULL;
}


void StreamServer::startAccept()
{
	socket_ptr socket = make_shared<tcp::socket>(*io_service_);
	acceptor_->async_accept(*socket, bind(&StreamServer::handleAccept, this, socket));
}


void StreamServer::handleAccept(socket_ptr socket)
{
	struct timeval tv;
	tv.tv_sec  = 5;
	tv.tv_usec = 0;
	setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(socket->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	logS(kLogNotice) << "StreamServer::NewConnection: " << socket->remote_endpoint().address().to_string() << endl;
	shared_ptr<StreamSession> session = make_shared<StreamSession>(this, socket);
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		session->setBufferMs(settings_.bufferMs);
		session->start();
		sessions_.insert(session);
	}
	startAccept();
}


void StreamServer::start()
{
	controlServer_.reset(new ControlServer(io_service_, settings_.controlPort, this));
	controlServer_->start();

	pcmReader_ .reset(new PipeReader(this, settings_.sampleFormat, settings_.codec, settings_.fifoName, settings_.pipeReadMs));
	pcmReader_->start();
	acceptor_ = make_shared<tcp::acceptor>(*io_service_, tcp::endpoint(tcp::v4(), settings_.port));
	startAccept();
}


void StreamServer::stop()
{
	controlServer_->stop();
	acceptor_->cancel();
	pcmReader_->stop();
	std::unique_lock<std::mutex> mlock(mutex_);
	for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
		(*it)->stop();
}

