#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <string>
#include <thread>
#include <atomic>
#include "serverConnection.h"


class Controller : public MessageReceiver
{
public:
	Controller();
	void start();
	void stop();
	virtual void onMessageReceived(BaseMessage* message);

private:
	void worker();
	std::atomic<bool> active_;
	std::thread* controllerThread;
};


#endif

