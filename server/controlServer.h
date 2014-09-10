#ifndef CONTROL_SERVER_H
#define CONTROL_SERVER_H

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include <memory>
#include <set>
#include <sstream>
#include "common/timeUtils.h"
#include "common/queue.h"
#include "common/message.h"
#include "common/headerMessage.h"
#include "common/sampleFormat.h"


using boost::asio::ip::tcp;
typedef std::shared_ptr<tcp::socket> socket_ptr;
using namespace std;



class ControlSession
{
public:
	ControlSession(socket_ptr sock);

	void start();
	void send(BaseMessage* message);
	bool isActive() const;

private:
	void sender();
	bool active_;
	socket_ptr socket_;
	thread* senderThread;
	Queue<shared_ptr<BaseMessage>> messages;
};



class ControlServer
{
public:
	ControlServer(unsigned short port);

	void start();
	void stop();

private:
	void acceptor();
	set<shared_ptr<ControlSession>> sessions;
	boost::asio::io_service io_service_;
	unsigned short port_;
	shared_ptr<HeaderMessage> headerChunk;
	shared_ptr<SampleFormat> sampleFormat;
	thread* acceptThread;
};



#endif


