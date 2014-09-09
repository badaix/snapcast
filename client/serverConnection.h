#ifndef RECEIVER_H
#define RECEIVER_H

#include <string>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include "stream.h"


using boost::asio::ip::tcp;


class MessageReceiver
{
public:
	virtual void onMessageReceived(tcp::socket* socket, const BaseMessage& baseMessage, char* buffer) = 0;
};


class ServerConnection
{
public:
	ServerConnection();
	void start(MessageReceiver* receiver, const std::string& ip, size_t port);
	void stop();

private:
	void socketRead(tcp::socket* socket, void* to, size_t bytes);
	void worker();

//	boost::asio::ip::tcp::endpoint endpt;
	MessageReceiver* messageReceiver;
	void getNextMessage(tcp::socket* socket);
	boost::asio::io_service io_service;
	tcp::resolver::iterator iterator;
	std::atomic<bool> active_;
	std::thread* receiverThread;
};


#endif

