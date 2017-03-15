/***
    This file is part of snapcast
    Copyright (C) 2014-2017  Johannes Pohl

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

#ifdef HAS_JSONRPCPP
#include <jsonrpcpp/jsonrp.hpp>
#else
#include "jsonrp.hpp"
#endif

#include "controlServer.h"
#include "message/time.h"
#include "common/log.h"
#include "common/utils.h"
#include "common/snapException.h"
#include "config.h"
#include <iostream>

using namespace std;

using json = nlohmann::json;


ControlServer::ControlServer(asio::io_service* io_service, size_t port, ControlMessageReceiver* controlMessageReceiver) : io_service_(io_service), port_(port), controlMessageReceiver_(controlMessageReceiver)
{
}


ControlServer::~ControlServer()
{
//	stop();
}


void ControlServer::cleanup()
{
	std::lock_guard<std::recursive_mutex> mlock(mutex_);
	for (auto it = sessions_.begin(); it != sessions_.end(); )
	{
		if (!(*it)->active())
		{
			logS(kLogErr) << "Session inactive. Removing\n";
			// don't block: remove ClientSession in a thread
			auto func = [](shared_ptr<ControlSession> s)->void{s->stop();};
			std::thread t(func, *it);
			t.detach();
			//(*it)->stop();
			sessions_.erase(it++);
		}
		else
			++it;
	}
}


void ControlServer::send(const std::string& message, const ControlSession* excludeSession)
{
	cleanup();
	for (auto s : sessions_)
	{
		if (s.get() != excludeSession)
			s->sendAsync(message);
	}
}


void ControlServer::onMessageReceived(ControlSession* connection, const std::string& message)
{
	std::lock_guard<std::recursive_mutex> mlock(mutex_);
	logD << "received: \"" << message << "\"\n";
	if ((message == "quit") || (message == "exit") || (message == "bye"))
	{
		for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
		{
			if (it->get() == connection)
			{
				sessions_.erase(it);
				break;
			}
		}
	}
	else
	{
		if (controlMessageReceiver_ != NULL)
			controlMessageReceiver_->onMessageReceived(connection, message);
	}
}



void ControlServer::startAccept()
{
	socket_ptr socket = make_shared<tcp::socket>(*io_service_);
	acceptor_->async_accept(*socket, bind(&ControlServer::handleAccept, this, socket));
}


void ControlServer::handleAccept(socket_ptr socket)
{
	struct timeval tv;
	tv.tv_sec  = 5;
	tv.tv_usec = 0;
	setsockopt(socket->native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(socket->native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
//	socket->set_option(boost::asio::ip::tcp::no_delay(false));
	logS(kLogNotice) << "ControlServer::NewConnection: " << socket->remote_endpoint().address().to_string() << endl;
	shared_ptr<ControlSession> session = make_shared<ControlSession>(this, socket);
	{
		std::lock_guard<std::recursive_mutex> mlock(mutex_);
		session->start();
		sessions_.insert(session);
		cleanup();
	}
	startAccept();
}


void ControlServer::start()
{
	acceptor_ = make_shared<tcp::acceptor>(*io_service_, tcp::endpoint(tcp::v4(), port_));
	startAccept();
}


void ControlServer::stop()
{
	if (acceptor_)	
		acceptor_->cancel();
	std::lock_guard<std::recursive_mutex> mlock(mutex_);
	for (auto s: sessions_)
		s->stop();
}

