/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#include <memory>
#include <sys/stat.h>
#include <fcntl.h>

#include "encoder/encoderFactory.h"
#include "common/snapException.h"
#include "common/strCompat.h"
#include "pcmStream.h"
#include "common/log.h"


using namespace std;



PcmStream::PcmStream(PcmListener* pcmListener, const StreamUri& uri) : 
	active_(false), pcmListener_(pcmListener), uri_(uri), pcmReadMs_(20), state_(kIdle)
{
	EncoderFactory encoderFactory;
 	if (uri_.query.find("codec") == uri_.query.end())
		throw SnapException("Stream URI must have a codec");
	encoder_.reset(encoderFactory.createEncoder(uri_.query["codec"]));

	if (uri_.query.find("name") == uri_.query.end())
		throw SnapException("Stream URI must have a name");
	name_ = uri_.query["name"];

 	if (uri_.query.find("sampleformat") == uri_.query.end())
		throw SnapException("Stream URI must have a sampleformat");
	sampleFormat_ = SampleFormat(uri_.query["sampleformat"]);
	logO << "PcmStream sampleFormat: " << sampleFormat_.getFormat() << "\n";

 	if (uri_.query.find("buffer_ms") != uri_.query.end())
		pcmReadMs_ = cpt::stoul(uri_.query["buffer_ms"]);
}


PcmStream::~PcmStream()
{
	stop();
}


std::shared_ptr<msg::CodecHeader> PcmStream::getHeader()
{
	return encoder_->getHeader();
}


const StreamUri& PcmStream::getUri() const
{
	return uri_;
}


const std::string& PcmStream::getName() const
{
	return name_;
}


const std::string& PcmStream::getId() const
{
	return getName();
}


const SampleFormat& PcmStream::getSampleFormat() const
{
	return sampleFormat_;
}


void PcmStream::start()
{
	logD << "PcmStream start: " << sampleFormat_.getFormat() << "\n";
	encoder_->init(this, sampleFormat_);
	active_ = true;
	thread_ = thread(&PcmStream::worker, this);
}


void PcmStream::stop()
{
	if (!active_ && !thread_.joinable())
		return;
		
	active_ = false;
	cv_.notify_one();
	if (thread_.joinable())
		thread_.join();
}


bool PcmStream::sleep(int32_t ms)
{
	if (ms < 0)
		return true;
	std::unique_lock<std::mutex> lck(mtx_);
	return (!cv_.wait_for(lck, std::chrono::milliseconds(ms), [this] { return !active_; }));
}


ReaderState PcmStream::getState() const
{
	return state_;
}


void PcmStream::setState(const ReaderState& newState)
{
	if (newState != state_)
	{
		state_ = newState;
		if (pcmListener_)
			pcmListener_->onStateChanged(this, newState);
	}
}


void PcmStream::onChunkEncoded(const Encoder* encoder, msg::PcmChunk* chunk, double duration)
{
//	logO << "onChunkEncoded: " << duration << " us\n";
	if (duration <= 0)
		return;

	chunk->timestamp.sec = tvEncodedChunk_.tv_sec;
	chunk->timestamp.usec = tvEncodedChunk_.tv_usec;
	chronos::addUs(tvEncodedChunk_, duration * 1000);
	if (pcmListener_)
		pcmListener_->onChunkRead(this, chunk, duration);
}


json PcmStream::toJson() const
{
	string state("unknown");
	if (state_ == kIdle)
		state = "idle";
	else if (state_ == kPlaying)
		state = "playing";
	else if (state_ == kDisabled)
		state = "disabled";

	json j = {
		{"uri", uri_.toJson()},
		{"id", getId()},
		{"status", state}
	};
	return j;
}

