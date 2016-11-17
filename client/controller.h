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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <thread>
#include <atomic>
#include "decoder/decoder.h"
#include "message/message.h"
#include "message/serverSettings.h"
#include "player/pcmDevice.h"
#ifdef HAS_ALSA
#include "player/alsaPlayer.h"
#elif HAS_OPENSL
#include "player/openslPlayer.h"
#elif HAS_COREAUDIO
#include "player/coreAudioPlayer.h"
#endif
#include "clientConnection.h"
#include "stream.h"


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
	void start(const PcmDevice& pcmDevice, const std::string& host, size_t port, int latency);
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
	std::thread controllerThread_;
	SampleFormat sampleFormat_;
	PcmDevice pcmDevice_;
	int latency_;
	std::unique_ptr<ClientConnection> clientConnection_;
	std::shared_ptr<Stream> stream_;
	std::unique_ptr<Decoder> decoder_;
	std::unique_ptr<Player> player_;
	std::shared_ptr<msg::ServerSettings> serverSettings_;
	std::shared_ptr<msg::CodecHeader> headerChunk_;
	std::mutex receiveMutex_;

	std::string exception_;
	bool asyncException_;
};


#endif

