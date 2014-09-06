#include "controller.h"
#include <iostream>
#include <unistd.h>


using namespace std;


Controller::Controller() : MessageReceiver(), active_(false)
{
}


void Controller::onMessageReceived(BaseMessage* message)
{
	if (message->type == message_type::payload)
	{
/*		if (decoder.decode((PcmChunk*)message))
			stream_->addChunk((PcmChunk*)message);
		else
*/			delete message;
//cout << ", decoded: " << chunk->payloadSize << ", Duration: " << chunk->getDuration() << ", sec: " << chunk->tv_sec << ", usec: " << chunk->tv_usec/1000 << ", type: " << chunk->type << "\n";
	}
	else if (message->type == message_type::header)
	{
//		decoder.setHeader((HeaderMessage*)message);
	}
	else if (message->type == message_type::sampleformat)
	{
		SampleFormat* sampleFormat = (SampleFormat*)message;
		cout << "SampleFormat rate: " << sampleFormat->rate << ", bits: " << sampleFormat->bits << ", channels: " << sampleFormat->channels << "\n";
		delete sampleFormat;
	}
}


void Controller::start()
{

	controllerThread = new thread(&Controller::worker, this);
}


void Controller::stop()
{
	active_ = false;
}


void Controller::worker()
{
	active_ = true;	
	while (active_)
	{
		usleep(10000);
	}
}


