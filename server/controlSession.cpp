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

#include <iostream>
#include <mutex>
#include "controlSession.h"
#include "common/log.h"
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
	active_ = true;
	readerThread_ = new thread(&ControlSession::reader, this);
	writerThread_ = new thread(&ControlSession::writer, this);
}


void ControlSession::stop()
{
	std::unique_lock<std::mutex> mlock(mutex_);
	active_ = false;
	try
	{
		std::error_code ec;
		if (socket_)
		{
			socket_->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) logE << "Error in socket shutdown: " << ec.message() << "\n";
			socket_->close(ec);
			if (ec) logE << "Error in socket close: " << ec.message() << "\n";
		}
		if (readerThread_)
		{
			logD << "joining readerThread\n";
			readerThread_->join();
			delete readerThread_;
		}
		if (writerThread_)
		{
			logD << "joining writerThread\n";
			writerThread_->join();
			delete writerThread_;
		}
	}
	catch(...)
	{
	}
	readerThread_ = NULL;
	writerThread_ = NULL;
	socket_ = NULL;
	logD << "ControlSession stopped\n";
}



void ControlSession::sendAsync(const std::string& message)
{
	messages_.push(message);
}


bool ControlSession::send(const std::string& message) const
{
//	logO << "send: " << message->type << ", size: " << message->size << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
	std::unique_lock<std::mutex> mlock(mutex_);
	if (!socket_ || !active_)
		return false;
	asio::streambuf streambuf;
	std::ostream request_stream(&streambuf);
	request_stream << message << "\r\n";
	asio::write(*socket_.get(), streambuf);
//	logO << "done: " << message->type << ", size: " << message->size << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
	return true;
}


void ControlSession::reader()
{
	active_ = true;
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
		logS(kLogErr) << "Exception in ControlSession::reader(): " << e.what() << endl;
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
		logS(kLogErr) << "Exception in ControlSession::writer(): " << e.what() << endl;
	}
	active_ = false;
}


