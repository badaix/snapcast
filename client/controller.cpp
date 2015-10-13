/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#include "controller.h"
#include <iostream>
#include <string>
#include <memory>
#include <boost/tokenizer.hpp>
#include "decoder/oggDecoder.h"
#include "decoder/pcmDecoder.h"
#include "decoder/flacDecoder.h"
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


Controller::Controller() : MessageReceiver(), active_(false), sampleFormat_(NULL), decoder_(NULL), asyncException_(false)
{
}


void Controller::onException(ClientConnection* connection, const std::exception& exception)
{
	logE << "onException: " << exception.what() << "\n";
	exception_ = exception;
	asyncException_ = true;
}


void Controller::onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer)
{
	if (baseMessage.type == message_type::kWireChunk)
	{
		if ((stream_ != NULL) && (decoder_ != NULL))
		{
			msg::PcmChunk* pcmChunk = new msg::PcmChunk(*sampleFormat_, 0);
			pcmChunk->deserialize(baseMessage, buffer);
			//logD << "chunk: " << pcmChunk->payloadSize;
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
		double latency = (reply.received.sec - reply.sent.sec) + (reply.received.usec - reply.sent.usec) / 1000000.;
//		logO << "timeMsg: " << latency << "\n";
		TimeProvider::getInstance().setDiffToServer((reply.latency - latency) * 1000 / 2);
//		logO << "diff to server [ms]: " << (float)TimeProvider::getInstance().getDiffToServer<chronos::usec>().count() / 1000.f << "\n";
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
	msg::Request timeReq(kTime);
	clientConnection_->send(&timeReq);
	return true;
}


void Controller::start(const PcmDevice& pcmDevice, const std::string& ip, size_t port, size_t latency, unsigned char volume, size_t ctlport)
{
	ip_ = ip;
	pcmDevice_ = pcmDevice;
	latency_ = latency;
	volume_ = volume;
	acceptThread_ = NULL;
	clientConnection_ = new ClientConnection(this, ip, port);
	controllerThread_ = new thread(&Controller::worker, this);

	if (ctlport>0)
		acceptThread_ = new thread(&Controller::startCtlServer, this, ctlport);
}


void Controller::stop()
{
	logD << "Stopping Controller" << endl;
	active_ = false;
	controllerThread_->join();
	clientConnection_->stop();

	if (acceptThread_ != NULL) {
		acceptThread_->join();
		delete acceptThread_;
	}

	delete controllerThread_;
	delete clientConnection_;
}


void Controller::worker()
{
	active_ = true;
	decoder_ = NULL;
	stream_ = NULL;

	while (active_)
	{
		try
		{
			clientConnection_->start();
			msg::Request requestMsg(kServerSettings);
			shared_ptr<msg::ServerSettings> serverSettings(NULL);
			while (active_ && !(serverSettings = clientConnection_->sendReq<msg::ServerSettings>(&requestMsg)));
			logO << "ServerSettings buffer: " << serverSettings->bufferMs << "\n";

			requestMsg.request = kSampleFormat;
			while (active_ && !(sampleFormat_ = clientConnection_->sendReq<msg::SampleFormat>(&requestMsg)));
			logO << "SampleFormat rate: " << sampleFormat_->rate << ", bits: " << sampleFormat_->bits << ", channels: " << sampleFormat_->channels << "\n";

			requestMsg.request = kHeader;
			shared_ptr<msg::Header> headerChunk(NULL);
			while (active_ && !(headerChunk = clientConnection_->sendReq<msg::Header>(&requestMsg)));
			logO << "Codec: " << headerChunk->codec << "\n";
			if (headerChunk->codec == "ogg")
				decoder_ = new OggDecoder();
			else if (headerChunk->codec == "pcm")
				decoder_ = new PcmDecoder();
			else if (headerChunk->codec == "flac")
				decoder_ = new FlacDecoder();
			decoder_->setHeader(headerChunk.get());

			msg::Request timeReq(kTime);
			for (size_t n=0; n<50 && active_; ++n)
			{
				shared_ptr<msg::Time> reply = clientConnection_->sendReq<msg::Time>(&timeReq, chronos::msec(2000));
				if (reply)
				{
					double latency = (reply->received.sec - reply->sent.sec) + (reply->received.usec - reply->sent.usec) / 1000000.;
					TimeProvider::getInstance().setDiffToServer((reply->latency - latency) * 1000 / 2);
					usleep(100);
				}
			}
			logO << "diff to server [ms]: " << (float)TimeProvider::getInstance().getDiffToServer<chronos::usec>().count() / 1000.f << "\n";

			stream_ = new Stream(*sampleFormat_);
			stream_->setBufferLen(serverSettings->bufferMs - latency_);

			player_ = new Player(pcmDevice_, stream_, volume_);
			player_->start();

			msg::Command startStream("startStream");
			shared_ptr<msg::Ack> ackMsg(NULL);
			while (active_ && !(ackMsg = clientConnection_->sendReq<msg::Ack>(&startStream)));

			while (active_)
			{
				for (size_t n=0; n<10 && active_; ++n)
				{
					usleep(100*1000);
					if (asyncException_)
						throw exception_;
				}

				if (sendTimeSyncMessage(5000))
					logO << "time sync main loop\n";
//				shared_ptr<msg::Time> reply = clientConnection_->sendReq<msg::Time>(&timeReq);
//				if (reply)
//				{
//					double latency = (reply->received.sec - reply->sent.sec) + (reply->received.usec - reply->sent.usec) / 1000000.;
//					TimeProvider::getInstance().setDiffToServer((reply->latency - latency) * 1000 / 2);
//				}
			}
		}
		catch (const std::exception& e)
		{
			asyncException_ = false;
			logS(kLogErr) << "Exception in Controller::worker(): " << e.what() << endl;
			logO << "Stopping clientConnection" << endl;
			clientConnection_->stop();
			logO << "Deleting stream" << endl;
			if (stream_ != NULL)
				delete stream_;
			stream_ = NULL;
			if (decoder_ != NULL)
				delete decoder_;
			decoder_ = NULL;
			logO << "done" << endl;
			for (size_t n=0; (n<10) && active_; ++n)
				usleep(100*1000);
		}
	}
	logD << "Thread stopped\n";
}

void Controller::startCtlServer(size_t port)
{
	boost::asio::io_service io_service;
	tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), port));
	logO << "remote control: bind on port " << port << endl;
	while (active_) {
		tcp::socket sock(io_service);
		a.accept(sock);
		std::thread(&Controller::handleRequest, this, std::move(sock)).detach();
	}
}

void Controller::handleRequest(tcp::socket sock) {
	boost::asio::streambuf buff;
	boost::system::error_code error;
	read_until(sock, buff, "\n", error);

	std::istream str(&buff);
	std::string l;
	std::getline(str, l);
	std::vector<string> v;
	boost::tokenizer<> tok(l);
	for (auto && s : tok) { v.push_back(s); }

	std::string msg;
	bool err=false;
	if (v.size()==0) { err=true; }
	else if (v[0]=="volume") {
		if (v.size()==1) { err=true; }
		else {
			if (v[1]=="get") {
				msg = to_string(player_->getVolume())+"\n";
			} else if (v[1]=="set") {
				if (v.size()==2) { err=true; }
				else {
					int vol=100;
					try {
						vol = stoi(v[2]);
					} catch (const std::exception& e) {
						vol = -1;
					}
					if (vol<0 || vol>200)
						err=true;
					else
						player_->setVolume((unsigned char) vol);
						msg = "ok\n";
						logO << "set player volume to " << vol << endl;
				}

			}
		}
	} else { err=true; }

	if (err) msg = "invalid\nUsage: volume get | volume set VOL\n";

	boost::asio::write(sock, boost::asio::buffer(msg),
			boost::asio::transfer_all(), error);
	sock.close();
}
