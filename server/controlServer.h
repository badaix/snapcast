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

#ifndef CONTROL_SERVER_H
#define CONTROL_SERVER_H

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include <memory>
#include <set>
#include <sstream>
#include <mutex>

#include "serverSession.h"
#include "common/queue.h"
#include "message/message.h"
#include "message/header.h"
#include "message/sampleFormat.h"
#include "message/serverSettings.h"


using boost::asio::ip::tcp;
typedef std::shared_ptr<tcp::socket> socket_ptr;
using namespace std;


class ControlServer : public MessageReceiver
{
public:
	ControlServer(unsigned short port);

	void start();
	void stop();
	void send(shared_ptr<msg::BaseMessage> message);
	virtual void onMessageReceived(ServerSession* connection, const msg::BaseMessage& baseMessage, char* buffer);
	void setHeader(msg::Header* header);
	void setFormat(msg::SampleFormat* format);
	void setServerSettings(msg::ServerSettings* settings);

private:
	void acceptor();
	mutable std::mutex mutex_;
	set<shared_ptr<ServerSession>> sessions_;
	boost::asio::io_service io_service_;
	unsigned short port_;
	msg::Header* headerChunk_;
	msg::SampleFormat* sampleFormat_;
	msg::ServerSettings* serverSettings_;
	thread* acceptThread_;
	Queue<shared_ptr<msg::BaseMessage>> messages_;
};



#endif


