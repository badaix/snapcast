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

#ifndef PCM_CHUNK_H
#define PCM_CHUNK_H

#include <chrono>
#include "message.h"
#include "wireChunk.h"
#include "common/sampleFormat.h"


namespace msg
{

/**
 * Piece of PCM data with SampleFormat information
 * Has information about "when" recorded (start) and duration
 * frames can be read with "readFrames", which will also change the start time
 */
class PcmChunk : public WireChunk
{
public:
	PcmChunk(const SampleFormat& sampleFormat, size_t ms);
	PcmChunk(const PcmChunk& pcmChunk);
	PcmChunk();
	virtual ~PcmChunk();

	int readFrames(void* outputBuffer, size_t frameCount);
	int seek(int frames);

	virtual chronos::time_point_clk start() const
	{
		return chronos::time_point_clk(
				chronos::sec(timestamp.sec) +
				chronos::usec(timestamp.usec) +
				chronos::usec((chronos::usec::rep)(1000000. * ((double)idx_ / (double)format.rate)))
				);
	}

	inline chronos::time_point_clk end() const
	{
		return start() + durationLeft<chronos::usec>();
	}

	template<typename T>
	inline T duration() const
	{
		return std::chrono::duration_cast<T>(chronos::nsec((chronos::nsec::rep)(1000000 * getFrameCount() / format.msRate())));
	}

	template<typename T>
	inline T durationLeft() const
	{
		return std::chrono::duration_cast<T>(chronos::nsec((chronos::nsec::rep)(1000000 * (getFrameCount() - idx_) / format.msRate())));
	}

	inline bool isEndOfChunk() const
	{
		return idx_ >= getFrameCount();
	}

	inline size_t getFrameCount() const
	{
		return (payloadSize / format.frameSize);
	}

	inline size_t getSampleCount() const
	{
		return (payloadSize / format.sampleSize);
	}

	SampleFormat format;

private:
	uint32_t idx_;
};

}

#endif


