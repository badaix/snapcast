#include "controller.h"
#include <iostream>
#include <string>
#include <memory>
#include <unistd.h>
#include "oggDecoder.h"
#include "pcmDecoder.h"
#include "alsaPlayer.h"
#include "timeProvider.h"
#include "common/log.h"
#include "common/snapException.h"
#include "message/serverSettings.h"
#include "message/time.h"
#include "message/request.h"
#include "message/ack.h"
#include "message/command.h"

using namespace std;


Controller::Controller() : MessageReceiver(), active_(false), sampleFormat(NULL), decoder(NULL)
{
}


void Controller::onException(ClientConnection* connection, const std::exception& exception)
{
	logE << "onException: " << exception.what() << "\n";
}


void Controller::onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer)
{
	if (baseMessage.type == message_type::kPayload)
	{
		if ((stream != NULL) && (decoder != NULL))
		{
			msg::PcmChunk* pcmChunk = new msg::PcmChunk(*sampleFormat, 0);
			pcmChunk->deserialize(baseMessage, buffer);
//logD << "chunk: " << pcmChunk->payloadSize;
			if (decoder->decode(pcmChunk))
			{
				stream->addChunk(pcmChunk);
//logD << ", decoded: " << pcmChunk->payloadSize << ", Duration: " << pcmChunk->getDuration() << ", sec: " << pcmChunk->timestamp.sec << ", usec: " << pcmChunk->timestamp.usec/1000 << ", type: " << pcmChunk->type << "\n";
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
	logD << "Stopping\n";
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
			msg::Request requestMsg(kServerSettings);
			shared_ptr<msg::ServerSettings> serverSettings(NULL);
			while (active_ && !(serverSettings = clientConnection->sendReq<msg::ServerSettings>(&requestMsg)));
			logO << "ServerSettings buffer: " << serverSettings->bufferMs << "\n";

			requestMsg.request = kSampleFormat;
			while (active_ && !(sampleFormat = clientConnection->sendReq<msg::SampleFormat>(&requestMsg)));
			logO << "SampleFormat rate: " << sampleFormat->rate << ", bits: " << sampleFormat->bits << ", channels: " << sampleFormat->channels << "\n";

			requestMsg.request = kHeader;
			shared_ptr<msg::Header> headerChunk(NULL);
			while (active_ && !(headerChunk = clientConnection->sendReq<msg::Header>(&requestMsg)));
			logO << "Codec: " << headerChunk->codec << "\n";
			if (headerChunk->codec == "ogg")
				decoder = new OggDecoder();
			else if (headerChunk->codec == "pcm")
				decoder = new PcmDecoder();
			decoder->setHeader(headerChunk.get());

			msg::Request timeReq(kTime);
			for (size_t n=0; n<50 && active_; ++n)
			{
				shared_ptr<msg::Time> reply = clientConnection->sendReq<msg::Time>(&timeReq, chronos::msec(2000));
				if (reply)
				{
					double latency = (reply->received.sec - reply->sent.sec) + (reply->received.usec - reply->sent.usec) / 1000000.;
					TimeProvider::getInstance().setDiffToServer((reply->latency - latency) * 1000 / 2);
					usleep(1000);
				}
			}
			logO << "diff to server [ms]: " << TimeProvider::getInstance().getDiffToServer<chronos::msec>().count() << "\n";

			stream = new Stream(*sampleFormat);
			stream->setBufferLen(serverSettings->bufferMs - latency_);

			Player player(pcmDevice_, stream);
			player.start();

			msg::Command startStream("startStream");
			shared_ptr<msg::Ack> ackMsg(NULL);
			while (active_ && !(ackMsg = clientConnection->sendReq<msg::Ack>(&startStream)));

			while (active_)
			{
				usleep(500*1000);
//throw SnapException("timeout");
                shared_ptr<msg::Time> reply = clientConnection->sendReq<msg::Time>(&timeReq);
                if (reply)
                {
                    double latency = (reply->received.sec - reply->sent.sec) + (reply->received.usec - reply->sent.usec) / 1000000.;
                    TimeProvider::getInstance().setDiffToServer((reply->latency - latency) * 1000 / 2);
                }
			}
		}
		catch (const std::exception& e)
		{
			logS(kLogErr) << "Exception in Controller::worker(): " << e.what() << endl;
			logO << "Stopping clientConnection" << endl;
			clientConnection->stop();
			logO << "Deleting stream" << endl;
			if (stream != NULL)
				delete stream;
			stream = NULL;
			if (decoder != NULL)
				delete decoder;
			decoder = NULL;
			logO << "done" << endl;
			if (active_)
				usleep(500*1000);
		}
	}
	logD << "Thread stopped\n";
}



