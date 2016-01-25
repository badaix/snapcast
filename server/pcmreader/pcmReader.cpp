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
#include <unistd.h>

#include "../encoder/encoderFactory.h"
#include "common/snapException.h"
#include "common/compat.h"
#include "pcmReader.h"
#include "common/log.h"


using namespace std;



PcmReader::PcmReader(PcmListener* pcmListener, const ReaderUri& uri) : pcmListener_(pcmListener), uri_(uri), pcmReadMs_(20)
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
	logE << "PcmReader sampleFormat: " << sampleFormat_.getFormat() << "\n";

 	if (uri_.query.find("buffer_ms") != uri_.query.end())
		pcmReadMs_ = cpt::stoul(uri_.query["buffer_ms"]);
}


PcmReader::~PcmReader()
{
	stop();
}


msg::Header* PcmReader::getHeader()
{
	return encoder_->getHeader();
}


const ReaderUri& PcmReader::getUri() const
{
	return uri_;
}


const std::string& PcmReader::getName() const
{
	return name_;
}


const SampleFormat& PcmReader::getSampleFormat() const
{
	return sampleFormat_;
}


void PcmReader::start()
{
	logE << "PcmReader start: " << sampleFormat_.getFormat() << "\n";
	encoder_->init(this, sampleFormat_);
 	active_ = true;
	readerThread_ = thread(&PcmReader::worker, this);
}


void PcmReader::stop()
{
	if (active_)
	{
		active_ = false;
		readerThread_.join();
	}
}


void PcmReader::onChunkEncoded(const Encoder* encoder, msg::PcmChunk* chunk, double duration)
{
//	logO << "onChunkEncoded: " << duration << " us\n";
	if (duration <= 0)
		return;

	chunk->timestamp.sec = tvEncodedChunk_.tv_sec;
	chunk->timestamp.usec = tvEncodedChunk_.tv_usec;
	chronos::addUs(tvEncodedChunk_, duration * 1000);
	pcmListener_->onChunkRead(this, chunk, duration);
}

