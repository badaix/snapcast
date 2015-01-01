#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <boost/asio.hpp>
#include <condition_variable>
#include <set>
#include "message/message.h"
#include "common/queue.h"


using boost::asio::ip::tcp;


class ServerSession;

class MessageReceiver
{
public:
	virtual void onMessageReceived(ServerSession* connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
};


class ServerSession
{
public:
	ServerSession(MessageReceiver* receiver, std::shared_ptr<tcp::socket> socket);
	~ServerSession();
	void start();
	void stop();
	bool send(msg::BaseMessage* message);
	void add(std::shared_ptr<msg::BaseMessage> message);

	virtual bool active()
	{
		return active_;
	}

	virtual void setStreamActive(bool active)
	{
		streamActive_ = active;
	}


protected:
	void socketRead(void* _to, size_t _bytes);
	void getNextMessage();
	void reader();
	void writer();

	std::atomic<bool> active_;
	std::atomic<bool> streamActive_;
	mutable std::mutex mutex_;
	std::thread* readerThread_;
	std::thread* writerThread_;
	std::shared_ptr<tcp::socket> socket_;
	MessageReceiver* messageReceiver_;
	Queue<std::shared_ptr<msg::BaseMessage>> messages_;
};




#endif




