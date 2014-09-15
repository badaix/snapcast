#include "socketConnection.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <mutex>
#include "log.h"


#define PCM_DEVICE "default"

using namespace std;


SocketConnection::SocketConnection(MessageReceiver* _receiver) : active_(false), connected_(false), messageReceiver(_receiver), reqId(0)
{
}


SocketConnection::~SocketConnection()
{
}



void SocketConnection::socketRead(void* _to, size_t _bytes)
{
//	std::unique_lock<std::mutex> mlock(mutex_);
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


bool SocketConnection::send(BaseMessage* message)
{
//	std::unique_lock<std::mutex> mlock(mutex_);
//cout << "send: " << message->type << ", size: " << message->getSize() << "\n";
	if (!connected())
		return false;
//cout << "send: " << message->type << ", size: " << message->getSize() << "\n";
	boost::asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	tv t;
	message->sent = t;
	message->serialize(stream);
	boost::asio::write(*socket.get(), streambuf);
	return true;
}


shared_ptr<SerializedMessage> SocketConnection::sendRequest(BaseMessage* message, size_t timeout)
{
	shared_ptr<SerializedMessage> response(NULL);
	if (++reqId == 0)
		++reqId;
	message->id = reqId;
	shared_ptr<PendingRequest> pendingRequest(new PendingRequest(reqId));

	{
		std::unique_lock<std::mutex> mlock(mutex_);
		pendingRequests.insert(pendingRequest);
	}
//	std::mutex mtx;
	std::unique_lock<std::mutex> lck(m);
	send(message);
	if (pendingRequest->cv.wait_for(lck,std::chrono::milliseconds(timeout)) == std::cv_status::no_timeout)
	{
		response = pendingRequest->response;
	} 
	else
	{
		cout << "timeout while waiting for response to: " << reqId << "\n";
	}
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		pendingRequests.erase(pendingRequest);
	}
	return response;
}


void SocketConnection::getNextMessage()
{
//cout << "getNextMessage\n";
	BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
//cout << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(&buffer[0], baseMessage.size);
	tv t;
	baseMessage.received = t;
	
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		for (auto req: pendingRequests)
		{
			if (req->id == baseMessage.refersTo)
			{
//cout << "getNextMessage response: " << baseMessage.type << ", size: " << baseMessage.size << "\n";
//long latency = (baseMessage.received.sec - baseMessage.sent.sec) * 1000000 + (baseMessage.received.usec - baseMessage.sent.usec);
//cout << "latency: " << latency << "\n";
				req->response.reset(new SerializedMessage());
				req->response->message = baseMessage;
				req->response->buffer = (char*)malloc(baseMessage.size);
				memcpy(req->response->buffer, &buffer[0], baseMessage.size);
	std::unique_lock<std::mutex> lck(m);
				req->cv.notify_one();
				return;
			}
		}
	}

	if (messageReceiver != NULL)
		messageReceiver->onMessageReceived(this, baseMessage, &buffer[0]);
}




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
//cout << "connecting\n";
				socket.reset(new tcp::socket(io_service));
				struct timeval tv;
				tv.tv_sec  = 5; 
				tv.tv_usec = 0;         
				setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
				socket->connect(*iterator);
				connected_ = true;
//cout << "connected\n";
				std::clog << kLogNotice << "connected\n";// to " << ip << ":" << port << std::endl;
			}
			while(active_)
			{
				getNextMessage();
			}
		}
		catch (const std::exception& e)
		{
			connected_ = false;
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










