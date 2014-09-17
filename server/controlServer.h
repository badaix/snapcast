#ifndef CONTROL_SERVER_H
#define CONTROL_SERVER_H

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include <memory>
#include <set>
#include <sstream>

#include "serverConnection.h"
#include "common/timeUtils.h"
#include "common/queue.h"
#include "common/message.h"
#include "common/headerMessage.h"
#include "common/sampleFormat.h"
#include "common/serverSettings.h"


using boost::asio::ip::tcp;
typedef std::shared_ptr<tcp::socket> socket_ptr;
using namespace std;


class ControlServer : public MessageReceiver
{
public:
	ControlServer(unsigned short port);

	void start();
	void stop();
	virtual void onMessageReceived(SocketConnection* connection, const BaseMessage& baseMessage, char* buffer);
	void setHeader(HeaderMessage* header);
	void setFormat(SampleFormat* format);
	void setServerSettings(ServerSettings* settings);

private:
	void acceptor();
	set<shared_ptr<ServerConnection>> sessions;
	boost::asio::io_service io_service_;
	unsigned short port_;
	HeaderMessage* headerChunk;
	SampleFormat* sampleFormat;
	ServerSettings* serverSettings;
	thread* acceptThread;
};



#endif


