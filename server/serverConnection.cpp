#include "serverConnection.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <mutex>
#include "common/log.h"

using namespace std;




ServerConnection::ServerConnection(MessageReceiver* _receiver, std::shared_ptr<tcp::socket> _socket) : SocketConnection(_receiver)
{
	socket = _socket;
}


void ServerConnection::worker()
{
	active_ = true;
	try
	{
		while (active_)
		{
			getNextMessage();
		}
	}
	catch (const std::exception& e)
	{
		cout << kLogNotice << "Exception: " << e.what() << ", trying to reconnect" << std::endl;
	}
	active_ = false;
}










