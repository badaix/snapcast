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

#include <memory>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pcmReader.h"
#include "../encoder/encoderFactory.h"
#include "common/log.h"
#include "common/snapException.h"


using namespace std;




PcmReader::PcmReader(PcmListener* pcmListener, const msg::SampleFormat& sampleFormat, const std::string& codec, const std::string& fifoName, size_t pcmReadMs) : pcmListener_(pcmListener), sampleFormat_(sampleFormat), pcmReadMs_(pcmReadMs)
{
	EncoderFactory encoderFactory;
	encoder_.reset(encoderFactory.createEncoder(codec));
}


PcmReader::~PcmReader()
{
	stop();
	close(fd_);
}


msg::Header* PcmReader::getHeader()
{
	return encoder_->getHeader();
}


void PcmReader::start()
{
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

