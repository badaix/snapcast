#include "stream.h"
#include "timeUtils.h"
#include <iostream>
#include <string.h>
#include <unistd.h>

Stream::Stream() : sleep(0), lastPlayerChunk(NULL), median(0), shortMedian(0), lastUpdate(0), skip(0), idx(0)
{
	silentPlayerChunk = new PlayerChunk();
	playerChunk = (short*)malloc(sizeof(short)*PLAYER_CHUNK_SIZE);
	pBuffer = new DoubleBuffer<int>(30000 / PLAYER_CHUNK_MS);
	pShortBuffer = new DoubleBuffer<int>(5000 / PLAYER_CHUNK_MS);
	pLock = new std::unique_lock<std::mutex>(mtx);
	bufferMs = 500;
}


void Stream::setBufferLen(size_t bufferLenMs)
{
	bufferMs = bufferLenMs;
}


void Stream::addChunk(Chunk* chunk)
{
	Chunk* c = new Chunk(*chunk);
//	mutex.lock();
	chunks.push_back(c);
//	mutex.unlock();
	cv.notify_all();
}


Chunk* Stream::getNextChunk()
{
	Chunk* chunk = NULL;
	if (chunks.empty())
		cv.wait(*pLock);

//	mutex.lock();
	chunk = chunks.front();
//	mutex.unlock();
	return chunk;
}


void Stream::getSilentPlayerChunk(short* outputBuffer)
{
	memset(outputBuffer, 0, sizeof(short)*PLAYER_CHUNK_SIZE);
}


timeval Stream::getNextPlayerChunk(short* outputBuffer, int correction)
{
	Chunk* chunk = getNextChunk();
	timeval tv = getTimeval(chunk);

//std::cerr << "GetNextPlayerChunk: " << correction << "\n";		
//		int age(0);
//		age = getAge(*chunk) + outputBufferDacTime*1000 - bufferMs;
//		std::cerr << "age: " << age << " \tidx: " << chunk->idx << "\n"; 

/*		double factor = (double)PLAYER_CHUNK_MS / (double)(PLAYER_CHUNK_MS + correction);
	size_t idx(0);
	size_t idxCorrection(0);
	for (size_t n=0; n<PLAYER_CHUNK_SIZE/2; ++n)
	{
		idx = chunk->idx + 2*floor(n*factor) - idxCorrection;
//std::cerr << factor << "\t" << n << "\t" << idx << "\n";
		if (idx >= WIRE_CHUNK_SIZE)
		{
			idxCorrection = 2*floor(n*factor);
			idx = 0;
			chunks.pop_front();
			delete chunk;
			chunk = getNextChunk();
		}
		playerChunk->payload[2*n] = chunk->payload[idx];
		playerChunk->payload[2*n+1] = chunk->payload[idx + 1];
	}
	addMs(chunk, -PLAYER_CHUNK_MS - correction);
	chunk->idx = idx;
	if (idx >= WIRE_CHUNK_SIZE)
	{
		chunks.pop_front();
		delete chunk;
	}
*/

	if (correction != 0)
	{
		int factor = ceil((float)PLAYER_CHUNK_MS / (float)abs(correction));
		std::cerr << "Correction: " << correction << ", factor: " << factor << "\n";
		size_t idx(chunk->idx);
		size_t samples(0);
		for (size_t n=0; n<PLAYER_CHUNK_SIZE/2; ++n)
		{
			*(outputBuffer + 2*n) = chunk->payload[idx];
			*(outputBuffer + 2*n+1) = chunk->payload[idx + 1];
//std::cerr << 2*n << "\t" << idx << "\n";
			if (n % factor == 0)
			{
				if (correction < 0)
				{
					samples += 4;
					idx += 4;
				}
			}
			else
			{
				samples += 2;
				idx += 2;
			}
/*			if (correction > 0)
			{
				if (n % 2 != 0)
				{
					samples += 4;
					idx += 4;
				}
				else
				{
					samples += 2;
					idx += 2;
				}
			}
			else if (correction < 0)
			{
				if (n % 3 != 0)
				{
					samples += 2;
					idx += 2;
				}
			}
*/
			if (idx >= WIRE_CHUNK_SIZE)
			{
//std::cerr << "idx >= WIRE_CHUNK_SIZE: " << idx << "\t" << WIRE_CHUNK_SIZE << "\n";
				chunks.pop_front();
				delete chunk;
				chunk = getNextChunk();
				idx -= WIRE_CHUNK_SIZE;
			}
		}
//std::cerr << "Idx: " << chunk->idx << " => " << idx+2 << "\t" << WIRE_CHUNK_SIZE << "\t" << PLAYER_CHUNK_SIZE/2 << "\n";
		chunk->idx = idx;
//timeval chunkTv = getTimeval(chunk);

		timeval nextTv = tv;
		addMs(nextTv, samples / PLAYER_CHUNK_MS_SIZE);
		setTimeval(chunk, nextTv);
//timeval tvLater = getTimeval(chunk);
//std::cerr << "Diff: " << diff_ms(tv, nextTv) << "\t" << chunk->idx / PLAYER_CHUNK_MS_SIZE << "\n";
//std::cerr << timeToStr(tv) << "\n" << timeToStr(chunkTv) << "\n" << timeToStr(tvLater) << "\n";
		return tv;
	}


	size_t missing = PLAYER_CHUNK_SIZE;// + correction*PLAYER_CHUNK_MS_SIZE;
	if (chunk->idx + PLAYER_CHUNK_SIZE > WIRE_CHUNK_SIZE)
	{
//std::cerr << "chunk->idx + PLAYER_CHUNK_SIZE >= WIRE_CHUNK_SIZE: " << chunk->idx + PLAYER_CHUNK_SIZE << " >= " << WIRE_CHUNK_SIZE << "\n";
		if (outputBuffer != NULL)
			memcpy(outputBuffer, &chunk->payload[chunk->idx], sizeof(int16_t)*(WIRE_CHUNK_SIZE - chunk->idx));
		missing = chunk->idx + PLAYER_CHUNK_SIZE - WIRE_CHUNK_SIZE;
//		mutex.lock();
		chunks.pop_front();
//		mutex.unlock();
		delete chunk;
		
		chunk = getNextChunk();
	}

	if (outputBuffer != NULL)
		memcpy((outputBuffer + PLAYER_CHUNK_SIZE - missing), &chunk->payload[chunk->idx], sizeof(int16_t)*missing);

	timeval nextTv = tv;
	addMs(nextTv, PLAYER_CHUNK_MS);
	setTimeval(chunk, nextTv);
	chunk->idx += missing;
	if (chunk->idx >= WIRE_CHUNK_SIZE)
	{
//		mutex.lock();
		chunks.pop_front();
//		mutex.unlock();
		delete chunk;
	}

	return tv;
}


void Stream::getChunk(short* outputBuffer, double outputBufferDacTime, unsigned long framesPerBuffer)
{
	if (sleep != 0)
	{
		pBuffer->clear();
		pShortBuffer->clear();
		if (sleep < 0)
		{
//			std::cerr << "Sleep: " << sleep << "\n";
			sleep += PLAYER_CHUNK_MS;
			if (sleep > 0)
				sleep = 0;
			getSilentPlayerChunk(outputBuffer);
		}
		else
		{
			for (int i=0; i<(sleep / PLAYER_CHUNK_MS) + 1; ++i)
			{
//				std::cerr << "Sleep: " << sleep << "\n";
				getNextPlayerChunk(outputBuffer);
			}
			sleep = 0;
		}
		return;
	}
	
	int correction(1);
/*	if (pBuffer->full() && (abs(median) <= PLAYER_CHUNK_MS))
	{
		if (median >= PLAYER_CHUNK_MS / 2)
			correction = 1;
		else if (median <= -PLAYER_CHUNK_MS / 2)
			correction = -1;
		if (correction != 0)
		{
			pBuffer->clear();
			pShortBuffer->clear();
		}
	}		
*/
	timeval tv = getNextPlayerChunk(outputBuffer, correction);
	int age = getAge(tv) - bufferMs + outputBufferDacTime*1000;
	pBuffer->add(age);
	pShortBuffer->add(age);
//	std::cerr << "Chunk: " << age << "\t" << outputBufferDacTime*1000 << "\n";

	time_t now = time(NULL);
	if (now != lastUpdate)
	{
		lastUpdate = now;
		median = pBuffer->median();
		shortMedian = pShortBuffer->median();
		if (abs(age) > 700)
			sleep = age;
		else if (pShortBuffer->full() && (abs(shortMedian) > WIRE_CHUNK_MS))
			sleep = shortMedian;
		else if (pBuffer->full() && (abs(median) >= PLAYER_CHUNK_MS))//ceil(PLAYER_CHUNK_MS / 2) + 1))//; || (median+1 < -ceil(PLAYER_CHUNK_MS / 2))))
			sleep = median;
//		else if (pBuffer->full() && (median+1 < -floor(PLAYER_CHUNK_MS / 2)))
//			sleep = median;
//sleep = 0;
		if (sleep != 0)
			std::cerr << "Sleep: " << sleep << "\n";
sleep = 0;
		std::cerr << "Chunk: " << age << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << outputBufferDacTime*1000 << "\n";
	}
}


void Stream::sleepMs(int ms)
{
	if (ms > 0)
		usleep(ms * 1000);
}


