/***
    This file is part of snapcast
    Copyright (C) 2014-2017  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <iostream>
#include <string>
#include <memory>
#include "controller.h"
#if defined(HAS_OGG) && (defined(HAS_TREMOR) || defined(HAS_VORBIS))
#include "decoder/oggDecoder.h"
#endif
#include "decoder/pcmDecoder.h"
#include "decoder/flacDecoder.h"
#include "timeProvider.h"
#include "message/time.h"
#include "message/hello.h"
#include "common/snapException.h"
#include "common/log.h"

using namespace std;


Controller::Controller(size_t instance) : MessageReceiver(), 
	instance_(instance), 
	active_(false), 
	latency_(0), 
	stream_(nullptr), 
	decoder_(nullptr), 
	player_(nullptr), 
	serverSettings_(nullptr), 
	asyncException_(false)
{
}


void Controller::onException(ClientConnection* connection, const std::exception& exception)
{
	logE << "Controller::onException: " << exception.what() << "\n";
	exception_ = exception.what();
	asyncException_ = true;
}


void Controller::onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer)
{
	std::lock_guard<std::mutex> lock(receiveMutex_);
	if (baseMessage.type == message_type::kWireChunk)
	{
		if (stream_ && decoder_)
		{
			msg::PcmChunk* pcmChunk = new msg::PcmChunk(sampleFormat_, 0);
			pcmChunk->deserialize(baseMessage, buffer);
//			logD << "chunk: " << pcmChunk->payloadSize << ", sampleFormat: " << sampleFormat_.rate << "\n";
			if (decoder_->decode(pcmChunk))
			{
//TODO: do decoding in thread?
				stream_->addChunk(pcmChunk);
				//logD << ", decoded: " << pcmChunk->payloadSize << ", Duration: " << pcmChunk->getDuration() << ", sec: " << pcmChunk->timestamp.sec << ", usec: " << pcmChunk->timestamp.usec/1000 << ", type: " << pcmChunk->type << "\n";
			}
			else
				delete pcmChunk;
		}
	}
	else if (baseMessage.type == message_type::kTime)
	{
		msg::Time reply;
		reply.deserialize(baseMessage, buffer);
		TimeProvider::getInstance().setDiff(reply.latency, reply.received - reply.sent);// ToServer(diff / 2);
	}
	else if (baseMessage.type == message_type::kServerSettings)
	{
		serverSettings_.reset(new msg::ServerSettings());
		serverSettings_->deserialize(baseMessage, buffer);
		logO << "ServerSettings - buffer: " << serverSettings_->getBufferMs() << ", latency: " << serverSettings_->getLatency() << ", volume: " << serverSettings_->getVolume() << ", muted: " << serverSettings_->isMuted() << "\n";
		if (stream_ && player_)
		{
			player_->setVolume(serverSettings_->getVolume() / 100.);
			player_->setMute(serverSettings_->isMuted());
			stream_->setBufferLen(serverSettings_->getBufferMs() - serverSettings_->getLatency());
		}
	}
	else if (baseMessage.type == message_type::kCodecHeader)
	{
		headerChunk_.reset(new msg::CodecHeader());
		headerChunk_->deserialize(baseMessage, buffer);

		logO << "Codec: " << headerChunk_->codec << "\n";
		decoder_.reset(nullptr);
		stream_ = nullptr;
		player_.reset(nullptr);

		if (headerChunk_->codec == "pcm")
			decoder_.reset(new PcmDecoder());
#if defined(HAS_OGG) && (defined(HAS_TREMOR) || defined(HAS_VORBIS))
		else if (headerChunk_->codec == "ogg")
			decoder_.reset(new OggDecoder());
#endif
		else if (headerChunk_->codec == "flac")
			decoder_.reset(new FlacDecoder());
		else
			throw SnapException("codec not supported: \"" + headerChunk_->codec + "\"");

		sampleFormat_ = decoder_->setHeader(headerChunk_.get());
		logState << "sampleformat: " << sampleFormat_.rate << ":" << sampleFormat_.bits << ":" << sampleFormat_.channels << "\n";

		stream_ = make_shared<Stream>(sampleFormat_);
		stream_->setBufferLen(serverSettings_->getBufferMs() - latency_);

#ifdef HAS_ALSA
		player_.reset(new AlsaPlayer(pcmDevice_, stream_));
#elif HAS_OPENSL
		player_.reset(new OpenslPlayer(pcmDevice_, stream_));
#elif HAS_COREAUDIO
		player_.reset(new CoreAudioPlayer(pcmDevice_, stream_));
#else
		throw SnapException("No audio player support");
#endif
		player_->setVolume(serverSettings_->getVolume() / 100.);
		player_->setMute(serverSettings_->isMuted());
		player_->start();
	}

	if (baseMessage.type != message_type::kTime)
		if (sendTimeSyncMessage(1000))
			logD << "time sync onMessageReceived\n";
}


bool Controller::sendTimeSyncMessage(long after)
{
	static long lastTimeSync(0);
	long now = chronos::getTickCount();
	if (lastTimeSync + after > now)
		return false;

	lastTimeSync = now;
	msg::Time timeReq;
	clientConnection_->send(&timeReq);
	return true;
}


void Controller::start(const PcmDevice& pcmDevice, const std::string& host, size_t port, int latency)
{
	pcmDevice_ = pcmDevice;
	latency_ = latency;
	clientConnection_.reset(new ClientConnection(this, host, port));
	controllerThread_ = thread(&Controller::worker, this);
}


void Controller::stop()
{
	logD << "Stopping Controller" << endl;
	active_ = false;
	controllerThread_.join();
	clientConnection_->stop();
}


void Controller::worker()
{
	active_ = true;

	while (active_)
	{
		try
		{
			clientConnection_->start();

			msg::Hello hello(clientConnection_->getMacAddress(), instance_);
			clientConnection_->send(&hello);

			msg::Time timeReq;
			for (size_t n=0; n<50 && active_; ++n)
			{
				shared_ptr<msg::Time> reply = clientConnection_->sendReq<msg::Time>(&timeReq, chronos::msec(2000));
				if (reply)
				{
					TimeProvider::getInstance().setDiff(reply->latency, reply->received - reply->sent);
					chronos::usleep(100);
				}
			}
			logO << "diff to server [ms]: " << (float)TimeProvider::getInstance().getDiffToServer<chronos::usec>().count() / 1000.f << "\n";

			while (active_)
			{
				for (size_t n=0; n<10 && active_; ++n)
				{
					chronos::sleep(100);
					if (asyncException_)
						throw AsyncSnapException(exception_);
				}

				if (sendTimeSyncMessage(5000))
					logO << "time sync main loop\n";
			}
		}
		catch (const std::exception& e)
		{
			asyncException_ = false;
			logS(kLogErr) << "Exception in Controller::worker(): " << e.what() << endl;
			clientConnection_->stop();
			player_.reset();
			stream_.reset();
			decoder_.reset();
			for (size_t n=0; (n<10) && active_; ++n)
				chronos::sleep(100);
		}
	}
	logD << "Thread stopped\n";
}



