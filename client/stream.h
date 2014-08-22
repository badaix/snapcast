#ifndef STREAM_H
#define STREAM_H


#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include "doubleBuffer.h"
#include "common/chunk.h"
#include "common/timeUtils.h"
#include "common/queue.h"


class Stream
{
public:
	Stream(size_t hz, size_t channels, size_t bps);
	void addChunk(Chunk* chunk);
	void clearChunks();
	void getPlayerChunk(void* outputBuffer, double outputBufferDacTime, unsigned long framesPerBuffer);
	void setBufferLen(size_t bufferLenMs);
	void setLatency(size_t latency);
	size_t getSampleRate() const
	{
		return hz_;
	}
	size_t getChannels() const
	{
		return channels_;
	}

private:
	time_point_ms getNextPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer, int correction = 0);
	time_point_ms getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer);
	void updateBuffers(int age);
	void resetBuffers();

	size_t hz_;
	size_t channels_;
	size_t bytesPerSample_;
	size_t frameSize_;

	long lastTick;
	int sleep;
	
//	int correction;
	Queue<std::shared_ptr<Chunk>> chunks;
	DoubleBuffer<int>* pCardBuffer;
	DoubleBuffer<int>* pMiniBuffer;
	DoubleBuffer<int>* pBuffer;
	DoubleBuffer<int>* pShortBuffer;
	std::shared_ptr<Chunk> chunk;

	int median;
	int shortMedian;
	time_t lastUpdate;
	int bufferMs;
	int latencyMs;
};



#endif


