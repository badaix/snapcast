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

#include "json/jsonrpc.h"
#include "streamServer.h"
#include "message/time.h"
#include "message/request.h"
#include "message/hello.h"
#include "common/log.h"
#include "config.h"
#include <iostream>

using namespace std;

using json = nlohmann::json;


StreamServer::StreamServer(asio::io_service* io_service, const StreamServerSettings& streamServerSettings) : io_service_(io_service), settings_(streamServerSettings)
{
}


StreamServer::~StreamServer()
{
}


void StreamServer::onStateChanged(const PcmStream* pcmStream, const ReaderState& state)
{
	logO << "onStateChanged (" << pcmStream->getName() << "): " << state << "\n";
//	logO << pcmStream->toJson().dump(4);
	json notification = JsonNotification::getJson("Stream.OnUpdate", pcmStream->toJson());
	controlServer_->send(notification.dump(), NULL);
}


void StreamServer::onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk* chunk, double duration)
{
//	logO << "onChunkRead (" << pcmStream->getName() << "): " << duration << "ms\n";
	bool isDefaultStream(pcmStream == streamManager_->getDefaultStream().get());

	std::shared_ptr<const msg::BaseMessage> shared_message(chunk);
	std::lock_guard<std::mutex> mlock(sessionsMutex_);
	for (auto s : sessions_)
	{
		if (!s->pcmStream() && isDefaultStream)//->getName() == "default")
			s->add(shared_message);
		else if (s->pcmStream().get() == pcmStream)
			s->add(shared_message);
	}
}


void StreamServer::onResync(const PcmStream* pcmStream, double ms)
{
	logO << "onResync (" << pcmStream->getName() << "): " << ms << "ms\n";
}


void StreamServer::onDisconnect(StreamSession* streamSession)
{
	std::lock_guard<std::mutex> mlock(sessionsMutex_);
	std::shared_ptr<StreamSession> session = nullptr;

	for (auto s: sessions_)
	{
		if (s.get() == streamSession)
		{
			session = s;
			break;
		}
	}

	if (session == nullptr)
		return;

	logO << "onDisconnect: " << session->macAddress << "\n";
	ClientInfoPtr clientInfo = Config::instance().getClientInfo(streamSession->macAddress);
	logD << "sessions: " << sessions_.size() << "\n";
	// don't block: remove StreamSession in a thread
	auto func = [](shared_ptr<StreamSession> s)->void{s->stop();};
	std::thread t(func, session);
	t.detach();
	sessions_.erase(session);

	logD << "sessions: " << sessions_.size() << "\n";

	// notify controllers if not yet done
	if (!clientInfo || !clientInfo->connected)
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

		if (request.method == "Server.GetStatus")
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

			Host host;
			//TODO: Set MAC and IP
			Snapserver snapserver("Snapserver", VERSION);
			response = {
				{"server", {
					{"host", host.toJson()},//getHostName()},
					{"snapserver", snapserver.toJson()}
				}},
				{"clients", jClient},
				{"streams", streamManager_->toJson()}
			};
//			cout << response.dump(4);
		}
		else if (request.method == "Server.DeleteClient")
		{
			clientInfo = Config::instance().getClientInfo(request.getParam("client").get<string>(), false);
			if (clientInfo == nullptr)
				throw JsonInternalErrorException("Client not found", request.id);
			response = clientInfo->host.mac;
			Config::instance().remove(clientInfo);
			Config::instance().save();
			json notification = JsonNotification::getJson("Client.OnDelete", clientInfo->toJson());
			controlServer_->send(notification.dump(), controlSession);
			clientInfo = nullptr;
		}
		else if (request.method == "Client.SetVolume")
		{
			clientInfo->config.volume.percent = request.getParam<uint16_t>("volume", 0, 100);
			response = clientInfo->config.volume.percent;
		}
		else if (request.method == "Client.SetMute")
		{
			clientInfo->config.volume.muted = request.getParam<bool>("mute", false, true);
			response = clientInfo->config.volume.muted;
		}
		else if (request.method == "Client.SetStream")
		{
			string streamId = request.getParam("id").get<string>();
			PcmStreamPtr stream = streamManager_->getStream(streamId);
			if (stream == nullptr)
				throw JsonInternalErrorException("Stream not found", request.id);

			clientInfo->config.streamId = streamId;
			response = clientInfo->config.streamId;

			StreamSession* session = getStreamSession(request.getParam("client").get<string>());
			if (session != NULL)
			{
				session->add(stream->getHeader());
				session->setPcmStream(stream);
			}
		}
		else if (request.method == "Client.SetLatency")
		{
			clientInfo->config.latency = request.getParam<int>("latency", -10000, settings_.bufferMs);
			response = clientInfo->config.latency;
		}
		else if (request.method == "Client.SetName")
		{
			clientInfo->config.name = request.getParam("name").get<string>();
			response = clientInfo->config.name;
		}
		else
			throw JsonMethodNotFoundException(request.id);

		if (clientInfo != nullptr)
		{
			serverSettings.volume = clientInfo->config.volume.percent;
			serverSettings.muted = clientInfo->config.volume.muted;
			serverSettings.latency = clientInfo->config.latency;

			StreamSession* session = getStreamSession(request.getParam("client").get<string>());
			if (session != NULL)
				session->send(&serverSettings);

			Config::instance().save();
			json notification = JsonNotification::getJson("Client.OnUpdate", clientInfo->toJson());
			controlServer_->send(notification.dump(), controlSession);
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
	logD << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << ", sent: " << baseMessage.sent.sec << "," << baseMessage.sent.usec << ", recv: " << baseMessage.received.sec << "," << baseMessage.received.usec << "\n";
	if (baseMessage.type == message_type::kRequest)
	{
		msg::Request requestMsg;
		requestMsg.deserialize(baseMessage, buffer);
		logD << "request: " << requestMsg.request << "\n";
		if (requestMsg.request == kTime)
		{
			msg::Time timeMsg;
			timeMsg.refersTo = requestMsg.id;
			timeMsg.latency = (requestMsg.received.sec - requestMsg.sent.sec) + (requestMsg.received.usec - requestMsg.sent.usec) / 1000000.;
			logD << "Latency: " << timeMsg.latency << ", refers to: " << timeMsg.refersTo << "\n";
			connection->send(&timeMsg);

			// refresh connection state
			ClientInfoPtr client = Config::instance().getClientInfo(connection->macAddress);
			if (client != nullptr)
			{
				gettimeofday(&client->lastSeen, NULL);
				client->connected = true;
			}
		}
	}
	else if (baseMessage.type == message_type::kHello)
	{
		msg::Hello helloMsg;
		helloMsg.deserialize(baseMessage, buffer);
		connection->macAddress = helloMsg.getMacAddress();
		logO << "Hello from " << connection->macAddress << ", host: " << helloMsg.getHostName() << ", v" << helloMsg.getVersion()
			<< ", ClientName: " << helloMsg.getClientName() << ", OS: " << helloMsg.getOS() << ", Arch: " << helloMsg.getArch()
			<< ", Protocol version: " << helloMsg.getProtocolVersion() << "\n";

		logD << "request kServerSettings: " << connection->macAddress << "\n";
//		std::lock_guard<std::mutex> mlock(mutex_);
		ClientInfoPtr clientInfo = Config::instance().getClientInfo(connection->macAddress, true);
		if (clientInfo == nullptr)
		{
			logE << "could not get client info for MAC: " << connection->macAddress << "\n";
		}
		else
		{
			logD << "request kServerSettings\n";
			msg::ServerSettings serverSettings;
			serverSettings.volume = clientInfo->config.volume.percent;
			serverSettings.muted = clientInfo->config.volume.muted;
			serverSettings.latency = clientInfo->config.latency;
			serverSettings.refersTo = helloMsg.id;
			serverSettings.bufferMs = settings_.bufferMs;
			connection->send(&serverSettings);
		}

		ClientInfoPtr client = Config::instance().getClientInfo(connection->macAddress);
		client->host.ip = connection->getIP();
		client->host.name = helloMsg.getHostName();
		client->host.os = helloMsg.getOS();
		client->host.arch = helloMsg.getArch();
		client->snapclient.version = helloMsg.getVersion();
		client->snapclient.name = helloMsg.getClientName();
		client->snapclient.protocolVersion = helloMsg.getProtocolVersion();
		client->connected = true;
		gettimeofday(&client->lastSeen, NULL);

		// Assign and update stream
		PcmStreamPtr stream = streamManager_->getStream(client->config.streamId);
		if (stream == nullptr)
		{
			stream = streamManager_->getDefaultStream();
			client->config.streamId = stream->getUri().id();
		}
		Config::instance().save();

		//TODO: wording pcmStream vs stream
		connection->setPcmStream(stream);
		auto headerChunk = stream->getHeader();
		connection->send(headerChunk.get());

		json notification = JsonNotification::getJson("Client.OnConnect", client->toJson());
//		logO << notification.dump(4) << "\n";
		controlServer_->send(notification.dump());
	}
}


StreamSession* StreamServer::getStreamSession(const std::string& mac)
{
//	logO << "getStreamSession: " << mac << "\n";
	for (auto session: sessions_)
	{
//		logO << "getStreamSession, checking: " << session->macAddress << "\n";
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
		std::lock_guard<std::mutex> mlock(sessionsMutex_);
		session->setBufferMs(settings_.bufferMs);
		session->start();
		sessions_.insert(session);
	}
	startAccept();
}


void StreamServer::start()
{
//	throw SnapException("good");
	controlServer_.reset(new ControlServer(io_service_, settings_.controlPort, this));
	controlServer_->start();

	streamManager_.reset(new StreamManager(this, settings_.sampleFormat, settings_.codec, settings_.streamReadMs));
	//TODO: check uniqueness of the stream
	for (auto& streamUri: settings_.pcmStreams)
		logO << "Stream: " << streamManager_->addStream(streamUri)->getUri().toJson() << "\n";
//	throw SnapException("bad");

	streamManager_->start();

	acceptor_ = make_shared<tcp::acceptor>(*io_service_, tcp::endpoint(tcp::v4(), settings_.port));
	startAccept();
}


void StreamServer::stop()
{
	controlServer_->stop();
	acceptor_->cancel();

	streamManager_->stop();

//	std::lock_guard<std::mutex> mlock(sessionsMutex_);
	for (auto session: sessions_)//it = sessions_.begin(); it != sessions_.end(); ++it)
		session->stop();
}

