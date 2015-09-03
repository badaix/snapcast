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

#include "boost/lexical_cast.hpp"
#include "controlServer.h"
#include "message/time.h"
#include "message/ack.h"
#include "message/request.h"
#include "message/command.h"
#include "common/log.h"
#include "common/utils.h"
#include "common/snapException.h"
#include "jsonrpc.h"
#include "config.h"
#include <iostream>

using namespace std;

using json = nlohmann::json;


ControlServer::ControlServer(size_t port) : port_(port)
{
}


ControlServer::~ControlServer()
{
	stop();
}


void ControlServer::send(const std::string& message)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	for (auto it = sessions_.begin(); it != sessions_.end(); )
	{
		if (!(*it)->active())
		{
			logS(kLogErr) << "Session inactive. Removing\n";
			// don't block: remove ServerSession in a thread
			auto func = [](shared_ptr<ControlSession> s)->void{s->stop();};
			std::thread t(func, *it);
			t.detach();
			sessions_.erase(it++);
		}
		else
			++it;
	}

	for (auto s : sessions_)
		s->add(message);
}


void ControlServer::onMessageReceived(ControlSession* connection, const std::string& message)
{
	logO << "received: \"" << message << "\"\n";
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
		try
		{
			JsonRequest request;
			request.parse(message);

			//{"jsonrpc": "2.0", "method": "System.GetStatus", "id": 2}
			//{"jsonrpc": "2.0", "method": "System.GetStatus", "params": {"client": "00:21:6a:7d:74:fc"}, "id": 2}

			//{"jsonrpc": "2.0", "method": "Client.SetVolume", "params": {"client": "00:21:6a:7d:74:fc", "volume": 83}, "id": 2}
			//{"jsonrpc": "2.0", "method": "Client.SetLatency", "params": {"client": "00:21:6a:7d:74:fc", "latency": 10}, "id": 2}
			//{"jsonrpc": "2.0", "method": "Client.SetName", "params": {"client": "00:21:6a:7d:74:fc", "name": "living room"}, "id": 2}
			//{"jsonrpc": "2.0", "method": "Client.SetMute", "params": {"client": "00:21:6a:7d:74:fc", "mute": false}, "id": 2}

//curl -X POST -H "Content-Type: application/json" -d '{"jsonrpc": "2.0", "method": "Application.SetVolume", "params": {"volume":100}, "id": 1}' http://i3c.pla.lcl:8080/jsonrpc
//https://github.com/pla1/utils/blob/master/kodi_remote.desktop
//http://forum.fhem.de/index.php?topic=10075.130;wap2
//http://kodi.wiki/view/JSON-RPC_API/v6#Application.SetVolume


			logO << "method: " << request.method << ", " << "id: " << request.id << "\n";

			json response;

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
				int volume = request.getParam("volume").get<int>();
				logO << "client: " << request.getParam("client").get<string>() << ", volume: " << volume << "\n";
				response = volume;
			}
			else if (request.method == "Client.SetLatency")
			{
				int latency = request.getParam("latency").get<int>();
				logO << "client: " << request.getParam("client").get<string>() << ", latency: " << latency << "\n";
				response = latency;
			}
			else if (request.method == "Client.SetName")
			{
				string name = request.getParam("name").get<string>();
				logO << "client: " << request.getParam("client").get<string>() << ", name: " << name << "\n";
				response = name;
			}
			else if (request.method == "Client.SetMute")
			{
				bool mute = request.getParam("mute").get<bool>();
				logO << "client: " << request.getParam("client").get<string>() << ", mute: " << mute << "\n";
				response = mute;
			}
			else
				throw JsonMethodNotFoundException(request);

			connection->send(request.getResponse(response).dump());
		}
		catch (const JsonRequestException& e)
		{
			connection->send(e.getResponse().dump());
		}
	}
}



void ControlServer::startAccept()
{
	socket_ptr socket = make_shared<tcp::socket>(io_service_);
	acceptor_->async_accept(*socket, bind(&ControlServer::handleAccept, this, socket));
}


void ControlServer::handleAccept(socket_ptr socket)
{
	struct timeval tv;
	tv.tv_sec  = 5;
	tv.tv_usec = 0;
	setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(socket->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	logS(kLogNotice) << "ControlServer::NewConnection: " << socket->remote_endpoint().address().to_string() << endl;
	shared_ptr<ControlSession> session = make_shared<ControlSession>(this, socket);
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		session->start();
		sessions_.insert(session);
	}
	startAccept();
}


void ControlServer::start()
{
	acceptor_ = make_shared<tcp::acceptor>(io_service_, tcp::endpoint(tcp::v4(), port_));
	startAccept();
	acceptThread_ = thread(&ControlServer::acceptor, this);
}


void ControlServer::stop()
{
	acceptor_->cancel();
	io_service_.stop();
	acceptThread_.join();
	std::unique_lock<std::mutex> mlock(mutex_);
	for (auto it = sessions_.begin(); it != sessions_.end(); ++it)
		(*it)->stop();
}


void ControlServer::acceptor()
{
	io_service_.run();
}

