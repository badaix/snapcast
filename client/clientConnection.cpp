#include <boost/lexical_cast.hpp>
#include <iostream>
#include <mutex>
#include "common/log.h"
#include "clientConnection.h"
#include "common/utils.h"
#include "common/snapException.h"


using namespace std;


ClientConnection::ClientConnection(MessageReceiver* receiver, const std::string& ip, size_t port) : active_(false), connected_(false), messageReceiver_(receiver), reqId_(1), ip_(ip), port_(port), readerThread_(NULL), sumTimeout_(chronos::msec(0))
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
		len += socket_->read_some(boost::asio::buffer((char*)_to + len, toRead));
//cout << "len: " << len << ", error: " << error << endl;
		toRead = _bytes - len;
//		cout << "\\";
//		cout.flush();
	}
	while (toRead > 0);
}


void ClientConnection::start()
{
	tcp::resolver resolver(io_service_);
	tcp::resolver::query query(tcp::v4(), ip_, boost::lexical_cast<string>(port_));
	auto iterator = resolver.resolve(query);
	logO << "connecting\n";
	socket_.reset(new tcp::socket(io_service_));
//				struct timeval tv;
//				tv.tv_sec  = 5;
//				tv.tv_usec = 0;
//				cout << "socket: " << socket->native() << "\n";
//				setsockopt(socket->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//				setsockopt(socket->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	socket_->connect(*iterator);
	logO << "MAC: \"" << getMacAddress(socket_->native()) << "\"\n";
	connected_ = true;
	logS(kLogNotice) << "connected" << endl;
	active_ = true;
	readerThread_ = new thread(&ClientConnection::reader, this);
}


void ClientConnection::stop()
{
	connected_ = false;
	active_ = false;
	try
	{
		boost::system::error_code ec;
		if (socket_)
		{
			socket_->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) logE << "Error in socket shutdown: " << ec << endl;
			socket_->close(ec);
			if (ec) logE << "Error in socket close: " << ec << endl;
		}
		if (readerThread_)
		{
			logD << "joining readerThread\n";
			readerThread_->join();
			delete readerThread_;
		}
	}
	catch(...)
	{
	}
	readerThread_ = NULL;
	logD << "readerThread terminated\n";
}


bool ClientConnection::send(msg::BaseMessage* message)
{
//	std::unique_lock<std::mutex> mlock(mutex_);
//logD << "send: " << message->type << ", size: " << message->getSize() << "\n";
	if (!connected())
		return false;
//logD << "send: " << message->type << ", size: " << message->getSize() << "\n";
	boost::asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	tv t;
	message->sent = t;
	message->serialize(stream);
	boost::asio::write(*socket_.get(), streambuf);
	return true;
}


shared_ptr<msg::SerializedMessage> ClientConnection::sendRequest(msg::BaseMessage* message, const chronos::msec& timeout)
{
	shared_ptr<msg::SerializedMessage> response(NULL);
	if (++reqId_ == 10000)
		reqId_ = 1;
	message->id = reqId_;
//logD << "Req: " << reqId << "\n";
	shared_ptr<PendingRequest> pendingRequest(new PendingRequest(reqId_));

	{
		std::unique_lock<std::mutex> mlock(mutex_);
		pendingRequests_.insert(pendingRequest);
	}
	std::mutex m;
	std::unique_lock<std::mutex> lck(m);
	send(message);
	if (pendingRequest->cv.wait_for(lck,std::chrono::milliseconds(timeout)) == std::cv_status::no_timeout)
	{
		response = pendingRequest->response;
		sumTimeout_ = chronos::msec(0);
//logD << "Resp: " << pendingRequest->id << "\n";
	}
	else
	{
		sumTimeout_ += timeout;
		logO << "timeout while waiting for response to: " << reqId_ << ", timeout " << sumTimeout_.count() << "\n";
		if (sumTimeout_ > chronos::sec(10))
			throw SnapException("sum timeout exceeded 10s");
	}
	{
		std::unique_lock<std::mutex> mlock(mutex_);
		pendingRequests_.erase(pendingRequest);
	}
	return response;
}


void ClientConnection::getNextMessage()
{
	msg::BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
//logD << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(&buffer[0], baseMessage.size);
	tv t;
	baseMessage.received = t;

	{
		std::unique_lock<std::mutex> mlock(mutex_);
		{
			for (auto req: pendingRequests_)
			{
				if (req->id == baseMessage.refersTo)
				{
					req->response.reset(new msg::SerializedMessage());
					req->response->message = baseMessage;
					req->response->buffer = (char*)malloc(baseMessage.size);
					memcpy(req->response->buffer, &buffer[0], baseMessage.size);
					req->cv.notify_one();
					return;
				}
			}
		}
	}

	if (messageReceiver_ != NULL)
		messageReceiver_->onMessageReceived(this, baseMessage, &buffer[0]);
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
		if (messageReceiver_ != NULL)
			messageReceiver_->onException(this, e);			
	}
	catch (...)
	{
	}
	connected_ = false;
	active_ = false;
}




