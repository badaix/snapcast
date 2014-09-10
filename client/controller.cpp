#include "controller.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include "oggDecoder.h"
#include "pcmDecoder.h"
#include "player.h"


using namespace std;


Controller::Controller() : MessageReceiver(), active_(false), sampleFormat(NULL)
{
	decoder = new OggDecoder();
}


void Controller::onMessageReceived(SocketConnection* connection, const BaseMessage& baseMessage, char* buffer)
{
	if (baseMessage.type == message_type::payload)
	{
		if ((stream != NULL) && (decoder != NULL))
		{
			PcmChunk* pcmChunk = new PcmChunk(*sampleFormat, 0);
			pcmChunk->deserialize(baseMessage, buffer);
//cout << "chunk: " << pcmChunk->payloadSize;
			if (decoder->decode(pcmChunk))
			{
				stream->addChunk(pcmChunk);
//cout << ", decoded: " << pcmChunk->payloadSize << ", Duration: " << pcmChunk->getDuration() << ", sec: " << pcmChunk->tv_sec << ", usec: " << pcmChunk->tv_usec/1000 << ", type: " << pcmChunk->type << "\n";
			}
			else
				delete pcmChunk;
		}
	}
	else if (baseMessage.type == message_type::header)
	{
		if (decoder != NULL)
		{
			HeaderMessage* headerMessage = new HeaderMessage();
			headerMessage->deserialize(baseMessage, buffer);
			decoder->setHeader(headerMessage);
		}
	}
	else if (baseMessage.type == message_type::sampleformat)
	{
		sampleFormat = new SampleFormat();
		sampleFormat->deserialize(baseMessage, buffer);
		cout << "SampleFormat rate: " << sampleFormat->rate << ", bits: " << sampleFormat->bits << ", channels: " << sampleFormat->channels << "\n";
	}
}


void Controller::start(std::string& _ip, size_t _port, int _bufferMs)
{
	bufferMs = _bufferMs;

	connection = new ClientConnection(this, _ip, _port);
	connection->start();

	controllerThread = new thread(&Controller::worker, this);
}


void Controller::stop()
{
	active_ = false;
}


void Controller::worker()
{
//	Decoder* decoder;
	active_ = true;	

	while (sampleFormat == NULL)
		usleep(10000);
	stream = new Stream(SampleFormat(*sampleFormat));
	stream->setBufferLen(bufferMs);

	Player player(stream);
	player.start();

	while (active_)
	{
		usleep(10000);
	}
}



