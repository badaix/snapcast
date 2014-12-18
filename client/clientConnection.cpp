#include <boost/lexical_cast.hpp>
#include <iostream>
#include <mutex>
#include "common/log.h"
#include "clientConnection.h"
#include "common/utils.h"



using namespace std;


ClientConnection::ClientConnection(MessageReceiver* _receiver, const std::string& _ip, size_t _port) : active_(false), connected_(false), messageReceiver(_receiver), reqId(1), ip(_ip), port(_port), readerThread(NULL)
{
}


ClientConnection::~ClientConnection()
{
}



void ClientConnection::socketRead(void* _to, size_t _bytes)
{
//	std::unique_lock<std::mutex> mlock(mutex_);
	size_t toRead = _bytes;
	size_t len = 0;
	do
	{
//		cout << "/";
//		cout.flush();
//		boost::system::error_code error;
		len += socket->read_some(boost::asio::buffer((char*)_to + len, toRead));
//cout << "len: " << len << ", error: " << error << endl;
		toRead = _bytes - len;
//		cout << "\\";
//		cout.flush();
	}
	while (toRead > 0);
}


void ClientConnection::start()
{
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(tcp::v4(), ip, boost::lexical_cast<string>(port));
	iterator = resolver.resolve(query);
	cout << "connecting\n";
	socket.reset(new tcp::socket(io_service));
//				struct timeval tv;
//				tv.tv_sec  = 5;
//				tv.tv_usec = 0;
//				cout << "socket: " << socket->native() << "\n";
//				setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//				setsockopt(socket->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	socket->connect(*iterator);
	cout << "MAC: \"" << getMacAddress(socket->native()) << "\"\n";
	connected_ = true;
	cout << "connected\n";
	std::clog << kLogNotice << "connected\n";// to " << ip << ":" << port << std::endl;
	active_ = true;
	readerThread = new thread(&ClientConnection::reader, this);
}


void ClientConnection::stop()
{
	connected_ = false;
	active_ = false;
	try
	{
		boost::system::error_code ec;
		if (socket)
		{
			socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) cout << "Error in socket shutdown: " << ec << "\n";
			socket->close(ec);
			if (ec) cout << "Error in socket close: " << ec << "\n";
		}
		if (readerThread)
		{
			cout << "joining readerThread\n";
			readerThread->join();
			delete readerThread;
		}
	}
	catch(...)
	{
	}
	readerThread = NULL;
	cout << "readerThread terminated\n";
}


bool ClientConnection::send(BaseMessage* message)
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


shared_ptr<SerializedMessage> ClientConnection::sendRequest(BaseMessage* message, size_t timeout)
{
	shared_ptr<SerializedMessage> response(NULL);
	if (++reqId == 10000)
		reqId = 1;
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


void ClientConnection::getNextMessage()
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
		std::unique_lock<std::mutex> mlock(mutex_);//, std::defer_lock);
//		if (mlock.try_lock_for(std::chrono::milliseconds(1000)))
		{
			for (auto req: pendingRequests)
			{
				if (req->id == baseMessage.refersTo)
				{
					req->response.reset(new SerializedMessage());
					req->response->message = baseMessage;
					req->response->buffer = (char*)malloc(baseMessage.size);
					memcpy(req->response->buffer, &buffer[0], baseMessage.size);
	//				std::unique_lock<std::mutex> lck(m);
					req->cv.notify_one();
					return;
				}
			}
		}
	}

	if (messageReceiver != NULL)
		messageReceiver->onMessageReceived(this, baseMessage, &buffer[0]);
}



void ClientConnection::reader()
{
	try
	{
		while(active_)
		{
			getNextMessage();
		}
	}
	catch (const std::exception& e)
	{
		if (messageReceiver != NULL)
			messageReceiver->onException(this, e);			
	}
	catch (...)
	{
	}
	connected_ = false;
	active_ = false;
}




