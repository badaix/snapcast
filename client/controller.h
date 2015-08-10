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


class Controller : public MessageReceiver
{
public:
	Controller();
	void start(const PcmDevice& pcmDevice, const std::string& ip, size_t port, size_t latency);
	void stop();
	virtual void onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer);
	virtual void onException(ClientConnection* connection, const std::exception& exception);

private:
	void worker();
	std::atomic<bool> active_;
	std::thread* controllerThread_;
	ClientConnection* clientConnection_;
	Stream* stream_;
	std::string ip_;
	std::shared_ptr<msg::SampleFormat> sampleFormat_;
	Decoder* decoder_;
	PcmDevice pcmDevice_;
	size_t latency_;
};


#endif

