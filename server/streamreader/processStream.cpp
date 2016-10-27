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

#include <unistd.h>

#include "processStream.h"
#include "common/log.h"


using namespace std;




ProcessStream::ProcessStream(PcmListener* pcmListener, const StreamUri& uri) : PcmStream(pcmListener, uri)
{
}


ProcessStream::~ProcessStream()
{
}


void ProcessStream::worker()
{
	timeval tvChunk;
	std::unique_ptr<msg::PcmChunk> chunk(new msg::PcmChunk(sampleFormat_, pcmReadMs_));

	setState(kPlaying);

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
				size_t toRead = chunk->payloadSize;
				size_t count = 0;

//// read

				encoder_->encode(chunk.get());
				nextTick += pcmReadMs_;
				chronos::addUs(tvChunk, pcmReadMs_ * 1000);
				long currentTick = chronos::getTickCount();

				if (nextTick >= currentTick)
				{
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
