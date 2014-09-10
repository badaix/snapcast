#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <thread>
#include <atomic>
#include "common/message.h"
#include "decoder.h"
#include "socketConnection.h"


class Controller : public MessageReceiver
{
public:
	Controller();
	void start(std::string& _ip, size_t _port, int _bufferMs);
	void stop();
	virtual void onMessageReceived(SocketConnection* connection, const BaseMessage& baseMessage, char* buffer);

private:
	void worker();
	std::atomic<bool> active_;
	std::thread* controllerThread;
	ClientConnection* connection;
	SampleFormat* sampleFormat;
	Decoder* decoder;
	Stream* stream;
	int bufferMs;
};


#endif

