#include "serverConnection.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "common/log.h"
#include "common/message.h"
#include "common/headerMessage.h"


#define PCM_DEVICE "default"

using namespace std;


ServerConnection::ServerConnection() : active_(false)
{
}


void ServerConnection::socketRead(tcp::socket* socket, void* to, size_t bytes)
{
	size_t toRead = bytes;
	size_t len = 0;
	do 
	{
		len += socket->read_some(boost::asio::buffer((char*)to + len, toRead));
		toRead = bytes - len;
	}
	while (toRead > 0);
}


void ServerConnection::start(MessageReceiver* receiver, const std::string& ip, size_t port)
{
	messageReceiver = receiver;
//	endpt.address(boost::asio::ip::address::from_string(ip));
//	endpt.port((port));
    std::cout << "Endpoint IP:   " << endpt.address().to_string() << std::endl;
    std::cout << "Endpoint Port: " << endpt.port() << std::endl;
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(tcp::v4(), ip, boost::lexical_cast<string>(port));
	iterator = resolver.resolve(query);
	receiverThread = new thread(&ServerConnection::worker, this);
}


void ServerConnection::stop()
{
	active_ = false;
}


void ServerConnection::getNextMessage(tcp::socket* socket)
{
	BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);

	socketRead(socket, &buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
//cout << "type: " << baseMessage.type << ", size: " << baseMessage.size << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(socket, &buffer[0], baseMessage.size);
	
	if (messageReceiver != NULL)
		messageReceiver->onMessageReceived(socket, baseMessage, &buffer[0]);
}


void ServerConnection::worker() 
{
	active_ = true;
	while (active_)
	{
		try
		{
			tcp::socket s(io_service);
			s.connect(*iterator);
			struct timeval tv;
			tv.tv_sec  = 5; 
			tv.tv_usec = 0;         
			setsockopt(s.native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//			std::clog << kLogNotice << "connected to " << ip << ":" << port << std::endl;

			while(active_)
			{
				getNextMessage(&s);
			}
		}
		catch (const std::exception& e)
		{
			cout << kLogNotice << "Exception: " << e.what() << ", trying to reconnect" << std::endl;
			usleep(500*1000);
		}
	}
}



