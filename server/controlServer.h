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
#include "common/timeUtils.h"
#include "common/queue.h"
#include "message/message.h"
#include "message/headerMessage.h"
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
	void send(shared_ptr<BaseMessage> message);
	virtual void onMessageReceived(ServerSession* connection, const BaseMessage& baseMessage, char* buffer);
	void setHeader(HeaderMessage* header);
	void setFormat(SampleFormat* format);
	void setServerSettings(ServerSettings* settings);

private:
	void acceptor();
	mutable std::mutex mutex;
	set<shared_ptr<ServerSession>> sessions;
	boost::asio::io_service io_service_;
	unsigned short port_;
	HeaderMessage* headerChunk;
	SampleFormat* sampleFormat;
	ServerSettings* serverSettings;
	thread* acceptThread;
	Queue<shared_ptr<BaseMessage>> messages;
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


