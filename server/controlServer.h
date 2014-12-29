#ifndef CONTROL_SERVER_H
#define CONTROL_SERVER_H

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include <memory>
#include <set>
#include <sstream>
#include <mutex>

#include "serverSession.h"
#include "common/queue.h"
#include "message/message.h"
#include "message/header.h"
#include "message/sampleFormat.h"
#include "message/serverSettings.h"


using boost::asio::ip::tcp;
typedef std::shared_ptr<tcp::socket> socket_ptr;
using namespace std;


class ControlServer : public MessageReceiver
{
public:
	ControlServer(unsigned short port);

	void start();
	void stop();
	void send(shared_ptr<msg::BaseMessage> message);
	virtual void onMessageReceived(ServerSession* connection, const msg::BaseMessage& baseMessage, char* buffer);
	void setHeader(msg::Header* header);
	void setFormat(msg::SampleFormat* format);
	void setServerSettings(msg::ServerSettings* settings);

private:
	void acceptor();
	mutable std::mutex mutex;
	set<shared_ptr<ServerSession>> sessions;
	boost::asio::io_service io_service_;
	unsigned short port_;
	msg::Header* headerChunk;
	msg::SampleFormat* sampleFormat;
	msg::ServerSettings* serverSettings;
	thread* acceptThread;
	Queue<shared_ptr<msg::BaseMessage>> messages;
};


class ServerException : public std::exception
{
public:
	ServerException(const std::string& what) : what_(what)
	{
	}

	virtual ~ServerException() throw()
	{
	}

	virtual const char* what() const throw()
	{
		return what_.c_str();
	}

private:
	std::string what_;
};



#endif


