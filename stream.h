#ifndef STREAM_H
#define STREAM_H


#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>
#include "doubleBuffer.h"
#include "chunk.h"
#include "timeUtils.h"
#include "queue.h"


class Stream
{
public:
	Stream();
	void addChunk(Chunk* chunk);
	std::shared_ptr<Chunk> getNextChunk();
	time_point_ms getNextPlayerChunk(short* outputBuffer, int correction = 0);
	void getSilentPlayerChunk(short* outputBuffer);
	void getChunk(short* outputBuffer, double outputBufferDacTime, unsigned long framesPerBuffer);
	void setBufferLen(size_t bufferLenMs);

private:
	void sleepMs(int ms);

	int sleep;
//	std::deque<Chunk*> chunks;
//	std::mutex mtx;
//	std::mutex mutex;
//	std::unique_lock<std::mutex>* pLock;
//	std::condition_variable cv;
	Queue<std::shared_ptr<Chunk>> chunks;
	DoubleBuffer<int>* pMiniBuffer;
	DoubleBuffer<int>* pBuffer;
	DoubleBuffer<int>* pShortBuffer;
	std::shared_ptr<Chunk> chunk;

	int median;
	int shortMedian;
	time_t lastUpdate;
	int bufferMs;
};



#endif


