#include "controller.h"
#include <iostream>
#include <string>
#include <memory>
#include <unistd.h>
#include "oggDecoder.h"
#include "pcmDecoder.h"
#include "player.h"
#include "timeProvider.h"
#include "common/serverSettings.h"
#include "common/timeMsg.h"
#include "common/requestMsg.h"

using namespace std;


Controller::Controller() : MessageReceiver(), active_(false), streamClient(NULL), sampleFormat(NULL), decoder(NULL)
{
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
//cout << ", decoded: " << pcmChunk->payloadSize << ", Duration: " << pcmChunk->getDuration() << ", sec: " << pcmChunk->timestamp.sec << ", usec: " << pcmChunk->timestamp.usec/1000 << ", type: " << pcmChunk->type << "\n";
			}
			else
				delete pcmChunk;
		}
	}
}


void Controller::start(const std::string& _ip, size_t _port, int _bufferMs)
{
	bufferMs = _bufferMs;
	ip = _ip;

	controlConnection = new ClientConnection(this, ip, _port);
	controlConnection->start();

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
	decoder = NULL;

	while (active_)
	{
		try
		{
			RequestMsg requestMsg("serverSettings");
			shared_ptr<ServerSettings> serverSettings(NULL);
			while (!(serverSettings = controlConnection->sendReq<ServerSettings>(&requestMsg, 1000)));
			cout << "ServerSettings port: " << serverSettings->port << "\n";
			streamClient = new StreamClient(this, ip, serverSettings->port);

			requestMsg.request = "sampleFormat";
			while (!(sampleFormat = controlConnection->sendReq<SampleFormat>(&requestMsg, 1000)));
			cout << "SampleFormat rate: " << sampleFormat->rate << ", bits: " << sampleFormat->bits << ", channels: " << sampleFormat->channels << "\n";

			decoder = new OggDecoder();
			if (decoder != NULL)
			{
				requestMsg.request = "headerChunk";
				shared_ptr<HeaderMessage> headerChunk(NULL);
				while (!(headerChunk = controlConnection->sendReq<HeaderMessage>(&requestMsg, 1000)));
				decoder->setHeader(headerChunk.get());
			}

			RequestMsg timeReq("time");
			for (size_t n=0; n<10; ++n)
			{
				shared_ptr<TimeMsg> reply = controlConnection->sendReq<TimeMsg>(&timeReq, 2000);
				if (reply)
				{
					double latency = (reply->received.sec - reply->sent.sec) + (reply->received.usec - reply->sent.usec) / 1000000.;
					TimeProvider::getInstance().setDiffToServer((reply->latency - latency) * 1000 / 2);
					usleep(1000);
				}
			}

			streamClient->start();
			stream = new Stream(*sampleFormat);
			stream->setBufferLen(bufferMs);

			Player player(stream);
			player.start();

			try
			{		
				while (active_)
				{
					usleep(1000000);
					shared_ptr<TimeMsg> reply = controlConnection->sendReq<TimeMsg>(&timeReq, 1000);
					if (reply)
					{
						double latency = (reply->received.sec - reply->sent.sec) + (reply->received.usec - reply->sent.usec) / 1000000.;
	//					cout << "C2S: " << timeMsg.latency << ", S2C: " << latency << ", diff: " << (timeMsg.latency - latency) / 2 << endl;
						TimeProvider::getInstance().setDiffToServer((reply->latency - latency) * 1000 / 2);
						cout << TimeProvider::getInstance().getDiffToServer() << "\n";
					}
				}
			}
			catch (const std::exception& e)
			{
				cout << "Stopping player\n";
				player.stop();
				cout << "Stopping streamClient\n";
				streamClient->stop();
				delete streamClient;
				streamClient = NULL;
				delete stream;
				stream = NULL;
				cout << "done\n";
				throw e;
			}
		}
		catch (const std::exception& e)
		{
			cout << "Exception in Controller::worker(): " << e.what() << "\n";
			if (decoder != NULL)
				delete decoder;
			decoder = NULL;
			usleep(1000000);
		}
	}
}



