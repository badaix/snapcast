//
// blocking_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip>
#include <thread>
#include <memory>
#include "chunk.h"
#include "timeUtils.h"
#include "queue.h"


using boost::asio::ip::tcp;

const int max_length = 1024;

typedef boost::shared_ptr<tcp::socket> socket_ptr;
using namespace std;
using namespace std::chrono;


std::string return_current_time_and_date()
{
    auto now = system_clock::now();
    auto in_time_t = system_clock::to_time_t(now);
	system_clock::duration ms = now.time_since_epoch();
	char buff[20];
	strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&in_time_t));
	stringstream ss;
	ss << buff << "." << std::setw(3) << std::setfill('0') << ((ms / milliseconds(1)) % 1000);
    return ss.str();
}


class Session
{
public:
	Session(socket_ptr sock) : socket_(sock)
	{
	}

	void sender()
	{
		try
		{
			for (;;)
			{
				shared_ptr<WireChunk> chunk(chunks.pop());
				boost::system::error_code error;
				size_t written = 0;
				do
				{
					written += boost::asio::write(*socket_, boost::asio::buffer(chunk.get() + written, sizeof(WireChunk) - written));//, error);
				}
				while (written < sizeof(WireChunk));

				if (error == boost::asio::error::eof)
					break; // Connection closed cleanly by peer.
				else if (error)
					throw boost::system::system_error(error); // Some other error.
			}
		}
		catch (std::exception& e)
		{
			std::cerr << "Exception in thread: " << e.what() << "\n";
		}
	}

	void start()
	{
		senderThread = new thread(&Session::sender, this);
//		readerThread.join();
	}

	void send(shared_ptr<WireChunk> chunk)
	{
		chunks.push(chunk);
	}

private:
	socket_ptr socket_;
	thread* senderThread;
	Queue<shared_ptr<WireChunk>> chunks;
};


class Server
{
public:
	Server(unsigned short port) : session(NULL), port_(port)
	{
	}

	void acceptor()
	{
		tcp::acceptor a(io_service_, tcp::endpoint(tcp::v4(), port_));
		for (;;)
		{
			socket_ptr sock(new tcp::socket(io_service_));
			a.accept(*sock);
			cout << "New connection: " << sock->remote_endpoint().address().to_string() << "\n";
			session = new Session(sock);
			session->start();
		}
	}
	
	void send(shared_ptr<WireChunk> chunk)
	{
		if (session != 0)
			session->send(chunk);
	}

	void start()
	{
		acceptThread = new thread(&Server::acceptor, this);
	}

	void stop()
	{
		acceptThread->join();
	}

private:
	Session* session;
	boost::asio::io_service io_service_;
	unsigned short port_;
	thread* acceptThread;
};


int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
			std::cerr << "Usage: blocking_tcp_echo_server <port>\n";
			return 1;
		}


		using namespace std; // For atoi.
		Server* server = new Server(atoi(argv[1]));
		server->start();

		char c[2];
		timeval tvChunk;
		gettimeofday(&tvChunk, NULL);
		long nextTick = getTickCount();

		while (cin.good())
		{
			shared_ptr<WireChunk> chunk(new WireChunk());
			for (size_t n=0; (n<WIRE_CHUNK_SIZE) && cin.good(); ++n)
			{
				c[0] = cin.get();
				c[1] = cin.get();
			    chunk->payload[n] = (int)c[0] + ((int)c[1] << 8);
			}

	//		if (!cin.good())
	//			cin.clear();

		    chunk->tv_sec = tvChunk.tv_sec;
		    chunk->tv_usec = tvChunk.tv_usec;
			server->send(chunk);

		    addMs(tvChunk, WIRE_CHUNK_MS);
			nextTick += WIRE_CHUNK_MS;
			long currentTick = getTickCount();
			if (nextTick > currentTick)
			{
				usleep((nextTick - currentTick) * 1000);
			}
			else
			{
				cin.sync();
				gettimeofday(&tvChunk, NULL);
				nextTick = getTickCount();
			}
		}
		return 0;
		server->stop();	
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}



