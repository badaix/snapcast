/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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
#include <cerrno>

#include "pipeStream.h"
#include "encoder/encoderFactory.h"
#include "common/snapException.h"
#include "common/strCompat.h"
#include "aixlog.hpp"


using namespace std;




PipeStream::PipeStream(PcmListener* pcmListener, const StreamUri& uri) : PcmStream(pcmListener, uri), fd_(-1)
{
	umask(0);
	string mode = uri_.getQuery("mode", "create");
		
	LOG(INFO) << "PipeStream mode: " << mode << "\n";
	if ((mode != "read") && (mode != "create"))
		throw SnapException("create mode for fifo must be \"read\" or \"create\"");
	
	if (mode == "create")
	{
		if ((mkfifo(uri_.path.c_str(), 0666) != 0) && (errno != EEXIST))
			throw SnapException("failed to make fifo \"" + uri_.path + "\": " + cpt::to_string(errno));
	}
}


PipeStream::~PipeStream()
{
	if (fd_ != -1)
		close(fd_);
}


void PipeStream::worker()
{
	timeval tvChunk;
	std::unique_ptr<msg::PcmChunk> chunk(new msg::PcmChunk(sampleFormat_, pcmReadMs_));
	string lastException = "";

	while (active_)
	{
		if (fd_ != -1)
			close(fd_);
		fd_ = open(uri_.path.c_str(), O_RDONLY | O_NONBLOCK);
		chronos::systemtimeofday(&tvChunk);
		tvEncodedChunk_ = tvChunk;
		long nextTick = chronos::getTickCount();
		int idleBytes = 0;
		int maxIdleBytes = sampleFormat_.rate*sampleFormat_.frameSize*dryoutMs_/1000;
		try
		{
			if (fd_ == -1)
				throw SnapException("failed to open fifo: \"" + uri_.path + "\"");

			while (active_)
			{
				chunk->timestamp.sec = tvChunk.tv_sec;
				chunk->timestamp.usec = tvChunk.tv_usec;
				int toRead = chunk->payloadSize;
				int len = 0;
				do
				{
					int count = read(fd_, chunk->payload + len, toRead - len);
					if (count < 0 && idleBytes < maxIdleBytes)
					{
						memset(chunk->payload + len, 0, toRead - len);
						idleBytes += toRead - len;
						len += toRead - len;
						continue;
					}
					if (count < 0)
					{
						setState(kIdle);
						if (!sleep(100))
							break;
					}
					else if (count == 0)
						throw SnapException("end of file");
					else 
					{
						len += count;
						idleBytes = 0;
					}
				}
				while ((len < toRead) && active_);

				if (!active_) break;

				/// TODO: use less raw pointers, make this encoding more transparent
				encoder_->encode(chunk.get());

				if (!active_) break;

				nextTick += pcmReadMs_;
				chronos::addUs(tvChunk, pcmReadMs_ * 1000);
				long currentTick = chronos::getTickCount();

				if (nextTick >= currentTick)
				{
					setState(kPlaying);
					if (!sleep(nextTick - currentTick))
						break;
				}
				else
				{
					chronos::systemtimeofday(&tvChunk);
					tvEncodedChunk_ = tvChunk;
					pcmListener_->onResync(this, currentTick - nextTick);
					nextTick = currentTick;
				}

				lastException = "";
			}
		}
		catch(const std::exception& e)
		{
			if (lastException != e.what())
			{
				LOG(ERROR) << "(PipeStream) Exception: " << e.what() << std::endl;
				lastException = e.what();
			}
			if (!sleep(100))
				break;
		}
	}
}
