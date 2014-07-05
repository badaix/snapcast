#ifndef STREAM_H
#define STREAM_H


#include <deque>
#include <mutex>
#include <condition_variable>
#include <vector>
#include "doubleBuffer.h"
#include "chunk.h"


class Stream
{
public:
	Stream();
	void addChunk(Chunk* chunk);
	Chunk* getNextChunk();
	PlayerChunk* getNextPlayerChunk(int correction = 0);
	PlayerChunk* getChunk(double outputBufferDacTime, unsigned long framesPerBuffer);

private:
	void sleepMs(int ms);


	std::deque<Chunk*> chunks;
	std::mutex mtx;
	std::unique_lock<std::mutex>* pLock;
	std::condition_variable cv;
	DoubleBuffer<int>* pBuffer;
	DoubleBuffer<int>* pShortBuffer;

	PlayerChunk* lastPlayerChunk;
	PlayerChunk* silentPlayerChunk;

	int median;
	int shortMedian;
	time_t lastUpdate;
	int bufferMs;
	int skip;
	size_t idx;
};



#endif


