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

#ifndef STREAM_H
#define STREAM_H

//#include <mutex>
//#include <condition_variable>
//#include <vector>
//#include <chrono>
//#include "common/timeUtils.h"

#include <deque>
#include <memory>
#include "doubleBuffer.h"
#include "message/message.h"
#include "message/pcmChunk.h"
#include "common/sampleFormat.h"
#include "common/queue.h"


/// Time synchronized audio stream
/**
 * Queue with PCM data.
 * Returns "online" server-time-synchronized PCM data
 */
class Stream
{
public:
	Stream(const SampleFormat& format);

	/// Adds PCM data to the queue
	void addChunk(msg::PcmChunk* chunk);
	void clearChunks();

	/// Get PCM data, which will be played out in "outputBufferDacTime" time
	/// frame = (num_channels) * (1 sample in bytes) = (2 channels) * (2 bytes (16 bits) per sample) = 4 bytes (32 bits)
	bool getPlayerChunk(void* outputBuffer, const chronos::usec& outputBufferDacTime, unsigned long framesPerBuffer);

	/// "Server buffer": playout latency, e.g. 1000ms
	void setBufferLen(size_t bufferLenMs);

	const SampleFormat& getFormat() const
	{
		return format_;
	}

	bool waitForChunk(size_t ms) const;

private:
	chronos::time_point_clk getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer);
	chronos::time_point_clk getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer, long framesCorrection);
	chronos::time_point_clk getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer);
	chronos::time_point_clk seek(long ms);
//	time_point_ms seekTo(const time_point_ms& to);
	void updateBuffers(int age);
	void resetBuffers();
	void setRealSampleRate(double sampleRate);

	SampleFormat format_;

	chronos::usec sleep_;

	Queue<std::shared_ptr<msg::PcmChunk>> chunks_;
//	DoubleBuffer<chronos::usec::rep> cardBuffer;
	DoubleBuffer<chronos::usec::rep> miniBuffer_;
	DoubleBuffer<chronos::usec::rep> buffer_;
	DoubleBuffer<chronos::usec::rep> shortBuffer_;
	std::shared_ptr<msg::PcmChunk> chunk_;

	int median_;
	int shortMedian_;
	time_t lastUpdate_;
	unsigned long playedFrames_;
	long correctAfterXFrames_;
	chronos::msec bufferMs_;
};



#endif


