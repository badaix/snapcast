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
	Stream();
	void addChunk(Chunk* chunk);
	void getPlayerChunk(short* outputBuffer, double outputBufferDacTime, unsigned long framesPerBuffer);
	void setBufferLen(size_t bufferLenMs);
	void setLatency(size_t latency);

private:
	time_point_ms getNextPlayerChunk(short* outputBuffer, unsigned long framesPerBuffer, int correction = 0);
	void getSilentPlayerChunk(short* outputBuffer, unsigned long framesPerBuffer);
	void updateBuffers(int age);
	void resetBuffers();

	long lastTick;
	float sleep;
	
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


