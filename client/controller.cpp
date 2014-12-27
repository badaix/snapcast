#include "controller.h"
#include <iostream>
#include <string>
#include <memory>
#include <unistd.h>
#include "oggDecoder.h"
#include "pcmDecoder.h"
#include "alsaPlayer.h"
#include "timeProvider.h"
#include "message/serverSettings.h"
#include "message/timeMsg.h"
#include "message/requestMsg.h"
#include "message/ackMsg.h"
#include "message/commandMsg.h"

using namespace std;


Controller::Controller() : MessageReceiver(), active_(false), sampleFormat(NULL), decoder(NULL)
{
}


void Controller::onException(ClientConnection* connection, const std::exception& exception)
{
	cout << "onException: " << exception.what() << "\n";
}


void Controller::onMessageReceived(ClientConnection* connection, const BaseMessage& baseMessage, char* buffer)
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
//cout << ", decoded: " << pcmChunk->payloadSize << ", Duration: " << pcmChunk->getDuration() << ", sec: " << pcmChunk->timestamp.sec << ", usec: " << pcmChunk->timestamp.usec/1000 << ", type: " << pcmChunk->type << "\n";
			}
			else
				delete pcmChunk;
		}
	}
}


void Controller::start(const PcmDevice& pcmDevice, const std::string& _ip, size_t _port, size_t latency)
{
	ip = _ip;
	pcmDevice_ = pcmDevice;
	latency_ = latency;
	clientConnection = new ClientConnection(this, ip, _port);
	controllerThread = new thread(&Controller::worker, this);
}


void Controller::stop()
{
	cout << "Stopping\n";
	active_ = false;
	controllerThread->join();
	clientConnection->stop();
	delete controllerThread;
	delete clientConnection;
}


void Controller::worker()
{
//	Decoder* decoder;
	active_ = true;
	decoder = NULL;
	stream = NULL;

	while (active_)
	{
		try
		{
			clientConnection->start();
			RequestMsg requestMsg(serversettings);
			shared_ptr<ServerSettings> serverSettings(NULL);
			while (active_ && !(serverSettings = clientConnection->sendReq<ServerSettings>(&requestMsg)));
			cout << "ServerSettings buffer: " << serverSettings->bufferMs << "\n";

			requestMsg.request = sampleformat;
			while (active_ && !(sampleFormat = clientConnection->sendReq<SampleFormat>(&requestMsg)));
			cout << "SampleFormat rate: " << sampleFormat->rate << ", bits: " << sampleFormat->bits << ", channels: " << sampleFormat->channels << "\n";

			requestMsg.request = header;
			shared_ptr<HeaderMessage> headerChunk(NULL);
			while (active_ && !(headerChunk = clientConnection->sendReq<HeaderMessage>(&requestMsg)));
			cout << "Codec: " << headerChunk->codec << "\n";
			if (headerChunk->codec == "ogg")
				decoder = new OggDecoder();
			else if (headerChunk->codec == "pcm")
				decoder = new PcmDecoder();
			decoder->setHeader(headerChunk.get());

			RequestMsg timeReq(timemsg);
			for (size_t n=0; n<50 && active_; ++n)
			{
				shared_ptr<TimeMsg> reply = clientConnection->sendReq<TimeMsg>(&timeReq, chronos::msec(2000));
				if (reply)
				{
					double latency = (reply->received.sec - reply->sent.sec) + (reply->received.usec - reply->sent.usec) / 1000000.;
					TimeProvider::getInstance().setDiffToServer((reply->latency - latency) * 1000 / 2);
					usleep(1000);
				}
			}
			cout << "diff to server [ms]: " << TimeProvider::getInstance().getDiffToServer<chronos::msec>().count() << "\n";

			stream = new Stream(*sampleFormat);
			stream->setBufferLen(serverSettings->bufferMs - latency_);

			Player player(pcmDevice_, stream);
			player.start();

			CommandMsg startStream("startStream");
			shared_ptr<AckMsg> ackMsg(NULL);
			while (active_ && !(ackMsg = clientConnection->sendReq<AckMsg>(&startStream)));

			while (active_)
			{
				usleep(500*1000);
                shared_ptr<TimeMsg> reply = clientConnection->sendReq<TimeMsg>(&timeReq);
                if (reply)
                {
                    double latency = (reply->received.sec - reply->sent.sec) + (reply->received.usec - reply->sent.usec) / 1000000.;
                    TimeProvider::getInstance().setDiffToServer((reply->latency - latency) * 1000 / 2);
                }
			}
		}
		catch (const std::exception& e)
		{
			cout << "Exception in Controller::worker(): " << e.what() << "\n";
			cout << "Deleting stream\n";
			if (stream != NULL)
				delete stream;
			stream = NULL;
			if (decoder != NULL)
				delete decoder;
			decoder = NULL;
			cout << "Stopping clientConnection\n";
			clientConnection->stop();
			cout << "done\n";
			if (active_)
				usleep(500*1000);
		}
	}
	cout << "Thread stopped\n";
}



