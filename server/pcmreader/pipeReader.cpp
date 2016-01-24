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

#include "pipeReader.h"
#include "../encoder/encoderFactory.h"
#include "common/log.h"
#include "common/snapException.h"


using namespace std;




PipeReader::PipeReader(PcmListener* pcmListener, const ReaderUri& uri) : PcmReader(pcmListener, uri)
{
	umask(0);
	mkfifo(uri_.path.c_str(), 0666);
	fd_ = open(uri_.path.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd_ == -1)
		throw SnapException("failed to open fifo: \"" + uri_.path + "\"");
}


PipeReader::~PipeReader()
{
	close(fd_);
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
					pcmListener_->onResync(this, currentTick - nextTick);
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
