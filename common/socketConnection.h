#ifndef SOCKET_CONNECTION_H
#define SOCKET_CONNECTION_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <boost/asio.hpp>
#include <condition_variable>
#include <set>
#include "message.h"


using boost::asio::ip::tcp;


class SocketConnection;


struct PendingRequest
{
	PendingRequest(uint16_t reqId) : id(reqId), response(NULL) {};

	uint16_t id;
	std::shared_ptr<SerializedMessage> response;
	std::condition_variable cv;
};


class MessageReceiver
{
public:
	virtual void onMessageReceived(SocketConnection* connection, const BaseMessage& baseMessage, char* buffer) = 0;
};


class SocketConnection
{
public:
	SocketConnection(MessageReceiver* _receiver);
	virtual ~SocketConnection();
	virtual void start();
	virtual void stop();
	virtual bool send(BaseMessage* _message);
	virtual std::shared_ptr<SerializedMessage> sendRequest(BaseMessage* message, size_t timeout);

	template <typename T>
	std::shared_ptr<T> sendReq(BaseMessage* message, size_t timeout)
	{
		std::shared_ptr<SerializedMessage> reply = sendRequest(message, timeout);
		if (!reply)
			return NULL;
		std::shared_ptr<T> msg(new T);
		msg->deserialize(reply->message, reply->buffer);
		return msg;
	}

	virtual bool active() 
	{
		return active_;
	}

	virtual bool connected() 
	{
		return (socket != 0);
//		return (connected_ && socket);
	}

protected:
	virtual void worker() = 0;

	void socketRead(void* _to, size_t _bytes);
	std::shared_ptr<tcp::socket> socket;

//	boost::asio::ip::tcp::endpoint endpt;
	std::atomic<bool> active_;
	std::atomic<bool> connected_;
	MessageReceiver* messageReceiver;
	void getNextMessage();
	boost::asio::io_service io_service;
	tcp::resolver::iterator iterator;
	std::thread* receiverThread;
	mutable std::mutex mutex_;
	std::mutex m;
   	std::set<std::shared_ptr<PendingRequest>> pendingRequests;
	uint16_t reqId;
};



#endif




