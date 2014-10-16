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
	Stream(const SampleFormat& format);
	void addChunk(PcmChunk* chunk);
	void clearChunks();
	bool getPlayerChunk(void* outputBuffer, const chronos::usec& outputBufferDacTime, unsigned long framesPerBuffer);
	void setBufferLen(size_t bufferLenMs);
	const SampleFormat& format;

private:
	chronos::time_point_hrc getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer);
	chronos::time_point_hrc getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer, long framesCorrection);
	chronos::time_point_hrc getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer);
	chronos::time_point_hrc seek(long ms);
//	time_point_ms seekTo(const time_point_ms& to);
	void updateBuffers(int age);
	void resetBuffers();
	void setRealSampleRate(double sampleRate);

	SampleFormat format_;

	long lastTick;
	chronos::usec sleep;

unsigned long playedSamples;
chronos::time_point_hrc playedSamplesTime;

	Queue<std::shared_ptr<PcmChunk>> chunks;
//	DoubleBuffer<chronos::usec::rep> cardBuffer;
	DoubleBuffer<chronos::usec::rep> miniBuffer;
	DoubleBuffer<chronos::usec::rep> buffer;
	DoubleBuffer<chronos::usec::rep> shortBuffer;
	std::shared_ptr<PcmChunk> chunk;

	int median;
	int shortMedian;
	time_t lastUpdate;
	chronos::msec bufferMs;
	unsigned long playedFrames;
	long correctAfterXFrames;
};



#endif


