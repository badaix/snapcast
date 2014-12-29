#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <boost/asio.hpp>
#include <condition_variable>
#include <set>
#include "message/message.h"
#include "common/timeDefs.h"


using boost::asio::ip::tcp;


class ClientConnection;


struct PendingRequest
{
	PendingRequest(uint16_t reqId) : id(reqId), response(NULL) {};

	uint16_t id;
	std::shared_ptr<msg::SerializedMessage> response;
	std::condition_variable cv;
};


class MessageReceiver
{
public:
	virtual void onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
	virtual void onException(ClientConnection* connection, const std::exception& exception) = 0;
};


class ClientConnection
{
public:
	ClientConnection(MessageReceiver* _receiver, const std::string& _ip, size_t _port);
	virtual ~ClientConnection();
	virtual void start();
	virtual void stop();
	virtual bool send(msg::BaseMessage* _message);
	virtual std::shared_ptr<msg::SerializedMessage> sendRequest(msg::BaseMessage* message, const chronos::msec& timeout = chronos::msec(1000));

	template <typename T>
	std::shared_ptr<T> sendReq(msg::BaseMessage* message, const chronos::msec& timeout = chronos::msec(1000))
	{
		std::shared_ptr<msg::SerializedMessage> reply = sendRequest(message, timeout);
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
	virtual void reader();

	void socketRead(void* _to, size_t _bytes);
	std::shared_ptr<tcp::socket> socket;

//	boost::asio::ip::tcp::endpoint endpt;
	std::atomic<bool> active_;
	std::atomic<bool> connected_;
	MessageReceiver* messageReceiver;
	void getNextMessage();
	boost::asio::io_service io_service;
	tcp::resolver::iterator iterator;
	mutable std::mutex mutex_;
	std::mutex m;
	std::set<std::shared_ptr<PendingRequest>> pendingRequests;
	uint16_t reqId;
	std::string ip;
	size_t port;
	std::thread* readerThread;
	chronos::msec sumTimeout;
};



#endif




