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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <thread>
#include <atomic>
#include "decoder/decoder.h"
#include "message/message.h"
#include "clientConnection.h"
#include "stream.h"
#include "pcmDevice.h"
#include "alsaPlayer.h"

using boost::asio::ip::tcp;

/// Forwards PCM data to the audio player
/**
 * Sets up a connection to the server (using ClientConnection)
 * Sets up the audio decoder and player. Decodes audio feeds PCM to the audio stream buffer
 * Does timesync with the server
 */
class Controller : public MessageReceiver
{
public:
	Controller();
	void start(const PcmDevice& pcmDevice, const std::string& ip, size_t port, size_t latency, unsigned char volume, size_t ctlport);
	void stop();

	/// Implementation of MessageReceiver.
	/// ClientConnection passes messages from the server through these callbacks
	virtual void onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer);

	/// Implementation of MessageReceiver.
	/// Used for async exception reporting
	virtual void onException(ClientConnection* connection, const std::exception& exception);

private:
	void worker();
	bool sendTimeSyncMessage(long after = 1000);
	std::atomic<bool> active_;
	std::thread* controllerThread_;
	ClientConnection* clientConnection_;
	Player* player_;
	Stream* stream_;
	std::string ip_;
	std::shared_ptr<msg::SampleFormat> sampleFormat_;
	Decoder* decoder_;
	PcmDevice pcmDevice_;
	size_t latency_;
	unsigned char volume_;

	std::exception exception_;
	bool asyncException_;

	void startCtlServer(size_t port);
	void handleRequest(tcp::socket sock);
	std::thread* acceptThread_;
};


#endif

