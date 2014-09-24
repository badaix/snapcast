#ifndef STREAM_H
#define STREAM_H


#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include <chrono>
#include "doubleBuffer.h"
#include "message/message.h"
#include "message/pcmChunk.h"
#include "message/sampleFormat.h"
#include "common/timeUtils.h"
#include "common/queue.h"


class Stream
{
public:
	Stream(const SampleFormat& format);
	void addChunk(PcmChunk* chunk);
	void clearChunks();
	bool getPlayerChunk(void* outputBuffer, chronos::usec outputBufferDacTime, unsigned long framesPerBuffer, size_t timeout);
	void setBufferLen(size_t bufferLenMs);
	const SampleFormat& format;

private:
	chronos::time_point_hrc getNextPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer, size_t timeout, int correction = 0);
	chronos::time_point_hrc getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer);
	chronos::time_point_hrc seek(long ms);
//	time_point_ms seekTo(const time_point_ms& to);
	void updateBuffers(int age);
	void resetBuffers();

	SampleFormat format_;

	long lastTick;
	long sleep;

	Queue<std::shared_ptr<PcmChunk>> chunks;
	DoubleBuffer<long> cardBuffer;
	DoubleBuffer<long> miniBuffer;
	DoubleBuffer<long> buffer;
	DoubleBuffer<long> shortBuffer;
	std::shared_ptr<PcmChunk> chunk;

	int median;
	int shortMedian;
	time_t lastUpdate;
	chronos::msec bufferMs;
};



#endif


