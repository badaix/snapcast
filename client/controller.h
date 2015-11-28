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

#include "browseAvahi.h"
#include <thread>
#include <atomic>
#include "decoder/decoder.h"
#include "message/message.h"
#include "clientConnection.h"
#include "stream.h"
#include "pcmDevice.h"


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
	void start(const PcmDevice& pcmDevice, const std::string& name, const std::string& ip, size_t port, size_t latency);
	void stop();

	/// Implementation of MessageReceiver.
	/// ClientConnection passes messages from the server through these callbacks
	virtual void onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer);

	/// Implementation of MessageReceiver.
	/// Used for async exception reporting
	virtual void onException(ClientConnection* connection, const std::exception& exception);
	PcmDevice pcmDevice_;
	size_t latency_;
        std::string name_;
	std::string ip_;
        size_t port_;


private:
	void worker();
	bool sendTimeSyncMessage(long after = 1000);
        void scan(const std::string serverName);
	std::atomic<bool> active_;
  std::atomic<bool> started_;
  std::atomic<bool> stopping_;
	std::thread* controllerThread_;
	ClientConnection* clientConnection_;
	Stream* stream_;
	std::shared_ptr<msg::SampleFormat> sampleFormat_;
	Decoder* decoder_;

	std::exception exception_;
	bool asyncException_;
};


#endif

