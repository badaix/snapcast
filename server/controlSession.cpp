/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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

#include <iostream>
#include <mutex>
#include "controlSession.h"
#include "aixlog.hpp"
#include "message/pcmChunk.h"

using namespace std;



ControlSession::ControlSession(ControlMessageReceiver* receiver, std::shared_ptr<tcp::socket> socket) : 
	active_(false), messageReceiver_(receiver)
{
	socket_ = socket;
}


ControlSession::~ControlSession()
{
	stop();
}


void ControlSession::start()
{
	{
		std::lock_guard<std::recursive_mutex> activeLock(activeMutex_);
		active_ = true;
	}
	readerThread_ = thread(&ControlSession::reader, this);
	writerThread_ = thread(&ControlSession::writer, this);
}


void ControlSession::stop()
{
	LOG(DEBUG) << "ControlSession::stop\n";
	std::lock_guard<std::recursive_mutex> activeLock(activeMutex_);
	active_ = false;
	try
	{
		std::error_code ec;
		if (socket_)
		{
			std::lock_guard<std::recursive_mutex> socketLock(socketMutex_);
			socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) LOG(ERROR) << "Error in socket shutdown: " << ec.message() << "\n";
			socket_->close(ec);
			if (ec) LOG(ERROR) << "Error in socket close: " << ec.message() << "\n";
		}
		if (readerThread_.joinable())
		{
			LOG(DEBUG) << "ControlSession joining readerThread\n";
			readerThread_.join();
		}
		if (writerThread_.joinable())
		{
			LOG(DEBUG) << "ControlSession joining writerThread\n";
			messages_.abort_wait();
			writerThread_.join();
		}
	}
	catch(...)
	{
	}
	socket_ = NULL;
	LOG(DEBUG) << "ControlSession ControlSession stopped\n";
}



void ControlSession::sendAsync(const std::string& message)
{
	messages_.push(message);
}


bool ControlSession::send(const std::string& message) const
{
	//LOG(INFO) << "send: " << message << ", size: " << message.length() << "\n";
	std::lock_guard<std::recursive_mutex> socketLock(socketMutex_);
	{
		std::lock_guard<std::recursive_mutex> activeLock(activeMutex_);
		if (!socket_ || !active_)
			return false;
	}
	asio::streambuf streambuf;
	std::ostream request_stream(&streambuf);
	request_stream << message << "\r\n";
	asio::write(*socket_.get(), streambuf);
	//LOG(INFO) << "done\n";
	return true;
}


void ControlSession::reader()
{
	try
	{
		std::stringstream message;
		while (active_)
		{
			asio::streambuf response;
			asio::read_until(*socket_, response, "\n");

			std::string s((istreambuf_iterator<char>(&response)), istreambuf_iterator<char>());
			message << s;
			if (s.empty() || (s.back() != '\n'))
				continue;

			string line;
			while (std::getline(message, line, '\n'))
			{
				if (line.empty())
					continue;

				size_t len = line.length() - 1;
				if ((len >= 2) && line[len-2] == '\r')
					--len;
				line.resize(len);
				if ((messageReceiver_ != NULL) && !line.empty())
					messageReceiver_->onMessageReceived(this, line);
			}
			message.str("");
			message.clear();
		}
	}
	catch (const std::exception& e)
	{
		SLOG(ERROR) << "Exception in ControlSession::reader(): " << e.what() << endl;
	}
	active_ = false;
}


void ControlSession::writer()
{
	try
	{
		asio::streambuf streambuf;
		std::ostream stream(&streambuf);
		string message;
		while (active_)
		{
			if (messages_.try_pop(message, std::chrono::milliseconds(500)))
				send(message);
		}
	}
	catch (const std::exception& e)
	{
		SLOG(ERROR) << "Exception in ControlSession::writer(): " << e.what() << endl;
	}
	active_ = false;
}


