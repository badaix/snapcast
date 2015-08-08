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

#include "pipeReader.h"
#include "encoderFactory.h"
#include "common/log.h"
#include "common/snapException.h"


using namespace std;




PipeReader::PipeReader(PipeListener* pipeListener, const msg::SampleFormat& sampleFormat, const std::string& codec, const std::string& fifoName) : pipeListener_(pipeListener), sampleFormat_(sampleFormat)
{
	umask(0);
	mkfifo(fifoName.c_str(), 0666);
	fd_ = open(fifoName.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd_ == -1)
		throw SnapException("failed to open fifo: \"" + fifoName + "\"");
	pcmReadMs_ = 20;
	EncoderFactory encoderFactory;
	encoder_.reset(encoderFactory.createEncoder(codec));
}


PipeReader::~PipeReader()
{
	stop();
	close(fd_);
}


msg::Header* PipeReader::getHeader()
{
	return encoder_->getHeader();
}


void PipeReader::start()
{
	encoder_->init(this, sampleFormat_);
 	active_ = true;
	readerThread_ = thread(&PipeReader::worker, this);
}


void PipeReader::stop()
{
	if (active_)
	{
		active_ = false;
		readerThread_.join();
	}
}


void PipeReader::onChunkEncoded(const Encoder* encoder, msg::PcmChunk* chunk, double duration)
{
//	logO << "onChunkEncoded: " << duration << " us\n";
	if (duration <= 0)
		return;

	chunk->timestamp.sec = tvEncodedChunk_.tv_sec;
	chunk->timestamp.usec = tvEncodedChunk_.tv_usec;
	chronos::addUs(tvEncodedChunk_, duration * 1000);
	pipeListener_->onChunkRead(this, chunk);
}


void PipeReader::worker()
{
	timeval tvChunk;
	std::unique_ptr<msg::PcmChunk> chunk(new msg::PcmChunk(sampleFormat_, pcmReadMs_));

	while (active_)
	{
		gettimeofday(&tvChunk, NULL);
		tvEncodedChunk_ = tvChunk;
		long nextTick = chronos::getTickCount();
		try
		{
			while (active_)
			{
				chunk->timestamp.sec = tvChunk.tv_sec;
				chunk->timestamp.usec = tvChunk.tv_usec;
				int toRead = chunk->payloadSize;
				int len = 0;
				do
				{
					int count = read(fd_, chunk->payload + len, toRead - len);

					if (count <= 0)
						usleep(100*1000);
					else
						len += count;
				}
				while ((len < toRead) && active_);

				encoder_->encode(chunk.get());
				nextTick += pcmReadMs_;
				chronos::addUs(tvChunk, pcmReadMs_ * 1000);
				long currentTick = chronos::getTickCount();

				if (nextTick >= currentTick)
				{
//					logO << "sleep: " << nextTick - currentTick << "\n";
					usleep((nextTick - currentTick) * 1000);
				}
				else
				{
					gettimeofday(&tvChunk, NULL);
					tvEncodedChunk_ = tvChunk;
					pipeListener_->onResync(this, currentTick - nextTick);
					nextTick = currentTick;
				}
			}
		}
		catch(const std::exception& e)
		{
			logE << "Exception: " << e.what() << std::endl;
		}
	}
}
