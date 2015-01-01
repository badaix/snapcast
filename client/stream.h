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


