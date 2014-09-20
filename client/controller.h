#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <thread>
#include <atomic>
#include "common/message.h"
#include "clientConnection.h"
#include "decoder.h"
#include "stream.h"


class Controller : public MessageReceiver
{
public:
	Controller();
	void start(const std::string& _ip, size_t _port, int _bufferMs);
	void stop();
	virtual void onMessageReceived(ClientConnection* connection, const BaseMessage& baseMessage, char* buffer);
	virtual void onException(ClientConnection* connection, const std::exception& exception);

private:
	void worker();
	std::atomic<bool> active_;
	std::thread* controllerThread;
	ClientConnection* clientConnection;
	Stream* stream;
	int bufferMs;
	std::string ip;
	std::shared_ptr<SampleFormat> sampleFormat;
	Decoder* decoder;
};


#endif

