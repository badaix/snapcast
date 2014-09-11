#ifndef SOCKET_CONNECTION_H
#define SOCKET_CONNECTION_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <boost/asio.hpp>
#include "message.h"


using boost::asio::ip::tcp;


class SocketConnection;


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
	virtual void send(BaseMessage* _message);
	virtual bool isActive() 
	{
		return active_;
	}

protected:
	virtual void worker() = 0;

	void socketRead(void* _to, size_t _bytes);
	std::shared_ptr<tcp::socket> socket;

//	boost::asio::ip::tcp::endpoint endpt;
	std::atomic<bool> active_;
	MessageReceiver* messageReceiver;
	void getNextMessage();
	boost::asio::io_service io_service;
	tcp::resolver::iterator iterator;
	std::thread* receiverThread;
	mutable std::mutex mutex_;
};


class ClientConnection : public SocketConnection
{
public:
	ClientConnection(MessageReceiver* _receiver, const std::string& _ip, size_t _port);

protected:
	virtual void worker();

private:
	std::string ip;
	size_t port;
};



class ServerConnection : public SocketConnection
{
public:
	ServerConnection(MessageReceiver* _receiver, std::shared_ptr<tcp::socket> _socket);

protected:
	virtual void worker();
};




#endif




