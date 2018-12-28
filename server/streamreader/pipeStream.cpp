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
#include <sys/select.h>

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
	int bufferMs_= 15000; // TODO: do not hardcode this. this should correspond to the bufferMs for streamsession

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
				bool data_in_this_cycle = false;
				do
				{
					fd_set rfds;
					FD_ZERO(&rfds);
					FD_SET(fd_, &rfds);
					struct timeval tv = {
						.tv_sec = bufferMs_ / 1000,
						.tv_usec = bufferMs_ % 1000,
					};

					int select_ret = select(fd_ + 1, &rfds, NULL, NULL, &tv);

					if (select_ret == -1) {
						perror("select()");
						throw SnapException("Error on select");
					}
					else if (select_ret)
					{
						int count = read(fd_, chunk->payload + len, toRead - len);
						if (count == 0)
						{
							LOG(INFO) << "EOF on input buffer" << endl;
							setState(kIdle);
							throw SnapException("end of file");
						}
						else
						{
							len += count;
							data_in_this_cycle = true;

							if (getState() != kPlaying)
							{
								setState(kPlaying);
								chronos::systemtimeofday(&tvChunk);
								tvEncodedChunk_ = tvChunk;
								pcmListener_->onResync(this, pcmReadMs_);
							}
						}
					}
					else
					{
						LOG(INFO) << "Did not receive data on input fifo -- timeout: " << bufferMs_  << "ms" << endl;
						setState(kIdle);
						memset(chunk->payload + len, 0, toRead - len);
						len += toRead - len;
					}
				}
				while ((len < toRead) && active_);

				if (!active_) break;

				/// TODO: use less raw pointers, make this encoding more transparent
				if (data_in_this_cycle) {
					encoder_->encode(chunk.get());
					if (!active_) break;
				}
				chronos::addUs(tvChunk, pcmReadMs_ * 1000);
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
