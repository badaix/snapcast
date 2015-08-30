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

#include "controlServer.h"
#include "message/time.h"
#include "message/ack.h"
#include "message/request.h"
#include "message/command.h"
#include "common/log.h"
#include "common/snapException.h"
#include "json.hpp"
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
		int id = -1;
		try
		{
			// http://www.jsonrpc.org/specification
			//	code	message	meaning
			//	-32700	Parse error	Invalid JSON was received by the server. An error occurred on the server while parsing the JSON text.
			//	-32600	Invalid Request	The JSON sent is not a valid Request object.
			//	-32601	Method not found	The method does not exist / is not available.
			//	-32602	Invalid params	Invalid method parameter(s).
			//	-32603	Internal error	Internal JSON-RPC error.
			//	-32000 to -32099	Server error	Reserved for implementation-defined server-errors.
			json request;
			try
			{
				try
				{
					request = json::parse(message);
				}
				catch (const exception& e)
				{
					throw SnapException(e.what(), -32700);
				}

				id = request["id"].get<int>();
				string jsonrpc = request["jsonrpc"].get<string>();
				if (jsonrpc != "2.0")
					throw SnapException("invalid jsonrpc value: " + jsonrpc, -32600);
				string method = request["method"].get<string>();
				if (method.empty())
					throw SnapException("method must not be empty", -32600);
				if (id < 0)
					throw SnapException("id must be a positive integer", -32600);

				json response = {
					{"jsonrpc", "2.0"},
					{"id", id}
				};

				if (method == "get")
				{
					response["result"] = "???";//nullptr;
				}
				else if (method == "set")
				{
					response["result"] = "234";//nullptr;
				}
				else
					throw SnapException("method not found: \"" + method + "\"", -32601);

				connection->send(response.dump());
			}
			catch (const SnapException& e)
			{
				throw;
			}
			catch (const exception& e)
			{
				throw SnapException(e.what(), -32603);
			}
		}
		catch (const SnapException& e)
		{
			int errorCode = e.errorCode();
			if (errorCode == 0)
				errorCode = -32603;

			json response = {
				{"jsonrpc", "2.0"},
				{"error", {
					{"code", errorCode},
					{"message", e.what()}
				}},
			};
			if (id == -1)
				response["id"] = nullptr;
			else
				response["id"] = id;

			connection->send(response.dump());
		}
	}
//get status
//get status server
//get status client[:MAC]
//set volume client[:MAC] {VOLUME}
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

