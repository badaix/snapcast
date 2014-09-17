#include "clientConnection.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <mutex>
#include "common/log.h"


using namespace std;


ClientConnection::ClientConnection(MessageReceiver* _receiver, const std::string& _ip, size_t _port) : SocketConnection(_receiver), ip(_ip), port(_port)
{
}


void ClientConnection::start()
{
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(tcp::v4(), ip, boost::lexical_cast<string>(port));
	iterator = resolver.resolve(query);
	SocketConnection::start();
}


void ClientConnection::worker()
{
	active_ = true;
	while (active_)
	{
		connected_ = false;
		try
		{
			{
//				std::unique_lock<std::mutex> mlock(mutex_);
				cout << "connecting\n";
				socket.reset(new tcp::socket(io_service));
				struct timeval tv;
				tv.tv_sec  = 5;
				tv.tv_usec = 0;
				cout << "socket: " << socket->native() << "\n";
				setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
				setsockopt(socket->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
				socket->connect(*iterator);
				connected_ = true;
				cout << "connected\n";
				std::clog << kLogNotice << "connected\n";// to " << ip << ":" << port << std::endl;
			}
			while(active_)
			{
//				cout << ".";
//				cout.flush();
				getNextMessage();
//				cout << "|";
//				cout.flush();
			}
		}
		catch (const std::exception& e)
		{
			connected_ = false;
			cout << kLogNotice << "Exception: " << e.what() << ", trying to reconnect" << std::endl;
			usleep(1000*1000);
		}
	}
}



