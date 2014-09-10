#include "socketConnection.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <mutex>
#include "common/log.h"


#define PCM_DEVICE "default"

using namespace std;


SocketConnection::SocketConnection(MessageReceiver* _receiver) : active_(false), messageReceiver(_receiver)
{
}


SocketConnection::~SocketConnection()
{
}



void SocketConnection::socketRead(void* _to, size_t _bytes)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	size_t toRead = _bytes;
	size_t len = 0;
	do 
	{
		len += socket->read_some(boost::asio::buffer((char*)_to + len, toRead));
		toRead = _bytes - len;
	}
	while (toRead > 0);
}


void SocketConnection::start()
{
	receiverThread = new thread(&SocketConnection::worker, this);
}


void SocketConnection::stop()
{
	active_ = false;
}


void SocketConnection::send(BaseMessage* message)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	boost::asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	for (;;)
	{
		message->serialize(stream);
		boost::asio::write(*socket.get(), streambuf);
	}
}


void SocketConnection::getNextMessage()
{
	BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
//cout << "type: " << baseMessage.type << ", size: " << baseMessage.size << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(&buffer[0], baseMessage.size);
	
	if (messageReceiver != NULL)
		messageReceiver->onMessageReceived(this, baseMessage, &buffer[0]);
}




ClientConnection::ClientConnection(MessageReceiver* _receiver, const std::string& _ip, size_t _port) : SocketConnection(_receiver), ip(_ip), port(_port)
{
//	endpt.address(boost::asio::ip::address::from_string(ip));
//	endpt.port((port));
//    std::cout << "Endpoint IP:   " << endpt.address().to_string() << std::endl;
//    std::cout << "Endpoint Port: " << endpt.port() << std::endl;
}


void ClientConnection::worker() 
{
	active_ = true;
	while (active_)
	{
		try
		{
			tcp::resolver resolver(io_service);
			tcp::resolver::query query(tcp::v4(), ip, boost::lexical_cast<string>(port));
			iterator = resolver.resolve(query);

			socket.reset(new tcp::socket(io_service));
			struct timeval tv;
			tv.tv_sec  = 5; 
			tv.tv_usec = 0;         
			setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
			socket->connect(*iterator);
			std::clog << kLogNotice << "connected\n";// to " << ip << ":" << port << std::endl;

			while(active_)
			{
				getNextMessage();
			}
		}
		catch (const std::exception& e)
		{
			cout << kLogNotice << "Exception: " << e.what() << ", trying to reconnect" << std::endl;
			usleep(500*1000);
		}
	}
}





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










