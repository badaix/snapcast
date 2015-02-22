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
#include "message/sampleFormat.h"
#include "common/queue.h"


class Stream
{
public:
	Stream(const msg::SampleFormat& format);
	void addChunk(msg::PcmChunk* chunk);
	void clearChunks();
	bool getPlayerChunk(void* outputBuffer, const chronos::usec& outputBufferDacTime, unsigned long framesPerBuffer);
	void setBufferLen(size_t bufferLenMs);
	const msg::SampleFormat& getFormat() const
	{
		return format_;
	}

private:
	chronos::time_point_hrc getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer);
	chronos::time_point_hrc getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer, long framesCorrection);
	chronos::time_point_hrc getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer);
	chronos::time_point_hrc seek(long ms);
//	time_point_ms seekTo(const time_point_ms& to);
	void updateBuffers(int age);
	void resetBuffers();
	void setRealSampleRate(double sampleRate);

	msg::SampleFormat format_;

	long lastTick_;
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
	chronos::msec bufferMs_;
	unsigned long playedFrames_;
	long correctAfterXFrames_;
};



#endif


