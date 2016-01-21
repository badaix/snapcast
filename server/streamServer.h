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

#ifndef STREAM_SERVER_H
#define STREAM_SERVER_H

#include <asio.hpp>
#include <vector>
#include <thread>
#include <memory>
#include <set>
#include <sstream>
#include <mutex>

#include "streamSession.h"
#include "pcmreader/pipeReader.h"
#include "common/queue.h"
#include "message/message.h"
#include "message/header.h"
#include "message/sampleFormat.h"
#include "message/serverSettings.h"
#include "controlServer.h"


using asio::ip::tcp;
typedef std::shared_ptr<tcp::socket> socket_ptr;


struct StreamServerSettings
{
	StreamServerSettings() :
		port(1704),
		controlPort(1705),
		codec("flac"),
		bufferMs(1000),
		sampleFormat("48000:16:2"),
		pipeReadMs(20)
	{
	}
	size_t port;
	size_t controlPort;
	std::vector<std::string> pcmStreams;
	std::string codec;
	int32_t bufferMs;
	msg::SampleFormat sampleFormat;
	size_t pipeReadMs;
};


/// Forwars PCM data to the connected clients
/**
 * Reads PCM data using PipeReader, implements PcmListener to get the (encoded) PCM stream.
 * Accepts and holds client connections (StreamSession)
 * Receives (via the MessageReceiver interface) and answers messages from the clients
 * Forwards PCM data to the clients
 */
class StreamServer : public MessageReceiver, ControlMessageReceiver, PcmListener
{
public:
	StreamServer(asio::io_service* io_service, const StreamServerSettings& streamServerSettings);
	virtual ~StreamServer();

	void start();
	void stop();

	/// Send a message to all connceted clients
	void send(const msg::BaseMessage* message);

	/// Clients call this when they receive a message. Implementation of MessageReceiver::onMessageReceived
	virtual void onMessageReceived(StreamSession* connection, const msg::BaseMessage& baseMessage, char* buffer);
	virtual void onDisconnect(StreamSession* connection);

	/// Implementation of ControllMessageReceiver::onMessageReceived, called by ControlServer::onMessageReceived
	virtual void onMessageReceived(ControlSession* connection, const std::string& message);

	/// Implementation of PcmListener
	virtual void onChunkRead(const PcmReader* pcmReader, const msg::PcmChunk* chunk, double duration);
	virtual void onResync(const PcmReader* pcmReader, double ms);

private:
	void startAccept();
	void handleAccept(socket_ptr socket);
	StreamSession* getStreamSession(const std::string& mac);
	mutable std::mutex mutex_;
	std::unique_ptr<PcmReader> pcmReader_;
	std::set<std::shared_ptr<StreamSession>> sessions_;
	asio::io_service* io_service_;
	std::shared_ptr<tcp::acceptor> acceptor_;

	StreamServerSettings settings_;
	msg::SampleFormat sampleFormat_;
	Queue<std::shared_ptr<msg::BaseMessage>> messages_;
	std::unique_ptr<ControlServer> controlServer_;
};



#endif


