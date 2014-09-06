#ifndef RECEIVER_H
#define RECEIVER_H

#include <string>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include "stream.h"
#include "oggDecoder.h"
#include "pcmDecoder.h"


using boost::asio::ip::tcp;


class MessageReceiver
{
public:
	virtual void onMessageReceived(BaseMessage* message) = 0;
};


class ServerConnection : public MessageReceiver
{
public:
	ServerConnection(Stream* stream);
	void start(MessageReceiver* receiver, const std::string& ip, int port);
	void stop();
	virtual void onMessageReceived(BaseMessage* message);

private:
	MessageReceiver* messageReceiver;
	BaseMessage* getNextMessage(tcp::socket* socket);
	void socketRead(tcp::socket* socket, void* to, size_t bytes);
	void worker();
	boost::asio::io_service io_service;
	tcp::resolver::iterator iterator;
	std::atomic<bool> active_;
	Stream* stream_;
	std::thread* receiverThread;
	OggDecoder decoder;
};


#endif

