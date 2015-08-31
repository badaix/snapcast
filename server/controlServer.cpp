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
#include "common/snapException.h"
#include "jsonrpc.h"
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
			JsonRequest jsonRequest;
			jsonRequest.parse(message);


			//{"jsonrpc": "2.0", "method": "get", "id": 2}
			//{"jsonrpc": "2.0", "method": "get", "params": ["status"], "id": 2}
			//{"jsonrpc": "2.0", "method": "get", "params": ["status", "server"], "id": 2}
			//{"jsonrpc": "2.0", "method": "get", "params": ["status", "client"], "id": 2}
			//{"jsonrpc": "2.0", "method": "get", "params": ["status", "client", "MAC"], "id": 2}
			//{"jsonrpc": "2.0", "method": "test", "params": ["status", "client", "MAC"], "id": 2}
			//{"jsonrpc": "2.0", "method": "set", "params": ["voume", "client", "MAC", "1.0"], "id": 2}

			//	-32601	Method not found	The method does not exist / is not available.
			//	-32602	Invalid params	Invalid method parameter(s).
			//	-32603	Internal error	Internal JSON-RPC error.

			logO << "method: " << jsonRequest.method << ", " << "id: " << jsonRequest.id << "\n";
			for (string s: jsonRequest.params)
				logO << "param: " << s << "\n";

			if (jsonRequest.params.empty())
				throw JsonInvalidParamsException(jsonRequest);

			vector<string>& params = jsonRequest.params;
			if (jsonRequest.method == "get")
			{
				if (params[0] == "status")
				{

				}
				else
					throw JsonInvalidParamsException(jsonRequest);
			}
			else if (jsonRequest.method == "set")
			{
				if (params[0] == "volume")
				{
					if ((params.size() < 4) || (params[1] != "client"))
						throw JsonInvalidParamsException(jsonRequest);
					try
					{
						double volume = boost::lexical_cast<double>(params[3]);
					}
					catch(...)
					{
						throw JsonInvalidParamsException(jsonRequest);
					}
				}
				else if (params[0] == "latency")
				{
					if ((params.size() < 3) || (params[1] != "client"))
						throw JsonInvalidParamsException(jsonRequest);
				}
				else
					throw JsonInvalidParamsException(jsonRequest);
			}
			else
				throw JsonMethodNotFoundException(jsonRequest);

			json response = {
				{"test", "123"},
				{"error", {
					{"code", 12},
					{"message", true}
				}}};

			connection->send(jsonRequest.getResponse(response).dump());
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

