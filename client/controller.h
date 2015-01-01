#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <thread>
#include <atomic>
#include "message/message.h"
#include "clientConnection.h"
#include "decoder.h"
#include "stream.h"
#include "pcmDevice.h"


class Controller : public MessageReceiver
{
public:
	Controller();
	void start(const PcmDevice& pcmDevice, const std::string& ip, size_t port, size_t latency);
	void stop();
	virtual void onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer);
	virtual void onException(ClientConnection* connection, const std::exception& exception);

private:
	void worker();
	std::atomic<bool> active_;
	std::thread* controllerThread_;
	ClientConnection* clientConnection_;
	Stream* stream_;
	std::string ip_;
	std::shared_ptr<msg::SampleFormat> sampleFormat_;
	Decoder* decoder_;
	PcmDevice pcmDevice_;
	size_t latency_;
};


#endif

