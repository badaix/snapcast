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

#include "controlSession.h"
#include "pipeReader.h"
#include "common/queue.h"
#include "message/message.h"
#include "message/header.h"
#include "message/sampleFormat.h"
#include "message/serverSettings.h"


using boost::asio::ip::tcp;
typedef std::shared_ptr<tcp::socket> socket_ptr;


/// Telnet like remote control
/**
 * Telnet like remote control
 */
class ControlServer : public ControlMessageReceiver
{
public:
	ControlServer(boost::asio::io_service* io_service, size_t port, ControlMessageReceiver* controlMessageReceiver = NULL);
	virtual ~ControlServer();

	void start();
	void stop();

	/// Send a message to all connceted clients
	void send(const std::string& message);

	/// Clients call this when they receive a message. Implementation of MessageReceiver::onMessageReceived
	virtual void onMessageReceived(ControlSession* connection, const std::string& message);

private:
	void startAccept();
	void handleAccept(socket_ptr socket);
//	void acceptor();
	mutable std::mutex mutex_;
	size_t port_;
	std::set<std::shared_ptr<ControlSession>> sessions_;
	boost::asio::io_service* io_service_;
	std::shared_ptr<tcp::acceptor> acceptor_;

//	std::thread acceptThread_;
	Queue<std::shared_ptr<msg::BaseMessage>> messages_;

	ControlMessageReceiver* controlMessageReceiver_;
};



#endif


