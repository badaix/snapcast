#include "stream.h"
#include "timeUtils.h"
#include <iostream>
#include <string.h>

Stream::Stream() : lastPlayerChunk(NULL), median(0), shortMedian(0), lastUpdate(0), skip(0), idx(0)
{
	silentPlayerChunk = new PlayerChunk();
	pBuffer = new DoubleBuffer<int>(30000 / PLAYER_CHUNK_MS);
	pShortBuffer = new DoubleBuffer<int>(5000 / PLAYER_CHUNK_MS);
	pLock = new std::unique_lock<std::mutex>(mtx);
	bufferMs = 500;
}


void Stream::addChunk(Chunk* chunk)
{
	Chunk* c = new Chunk(*chunk);
	chunks.push_back(c);
	cv.notify_all();
}


Chunk* Stream::getNextChunk()
{
	Chunk* chunk = NULL;
	if (chunks.empty())
		cv.wait(*pLock);

	chunk = chunks.front();
	return chunk;
}


PlayerChunk* Stream::getNextPlayerChunk(int correction)
{
	Chunk* chunk = getNextChunk();
	if (correction > PLAYER_CHUNK_MS / 2)
		correction = PLAYER_CHUNK_MS/2;
	else if (correction < -PLAYER_CHUNK_MS/2)
		correction = -PLAYER_CHUNK_MS/2;

//std::cerr << "GetNextPlayerChunk: " << correction << "\n";		
//		int age(0);
//		age = getAge(*chunk) + outputBufferDacTime*1000 - bufferMs;
//		std::cerr << "age: " << age << " \tidx: " << chunk->idx << "\n"; 
	PlayerChunk* playerChunk = new PlayerChunk();
	playerChunk->tv_sec = chunk->tv_sec;
	playerChunk->tv_usec = chunk->tv_usec;
	playerChunk->idx = 0;

	size_t missing = PLAYER_CHUNK_SIZE;// + correction*PLAYER_CHUNK_MS_SIZE;
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
	if (chunk->idx + PLAYER_CHUNK_SIZE > WIRE_CHUNK_SIZE)
	{
//std::cerr << "chunk->idx + PLAYER_CHUNK_SIZE >= WIRE_CHUNK_SIZE: " << chunk->idx + PLAYER_CHUNK_SIZE << " >= " << WIRE_CHUNK_SIZE << "\n";
		memcpy(&(playerChunk->payload[0]), &chunk->payload[chunk->idx], sizeof(int16_t)*(WIRE_CHUNK_SIZE - chunk->idx));
		missing = chunk->idx + PLAYER_CHUNK_SIZE - WIRE_CHUNK_SIZE;
		chunks.pop_front();
		delete chunk;
		
		chunk = getNextChunk();
	}

	memcpy(&(playerChunk->payload[0]), &chunk->payload[chunk->idx], sizeof(int16_t)*missing);

	addMs(chunk, -PLAYER_CHUNK_MS);
	chunk->idx += missing;
	if (chunk->idx >= WIRE_CHUNK_SIZE)
	{
		chunks.pop_front();
		delete chunk;
	}
	return playerChunk;
}


PlayerChunk* Stream::getChunk(double outputBufferDacTime, unsigned long framesPerBuffer)
{
	int correction(0);
	if (pBuffer->full() && (abs(median) <= PLAYER_CHUNK_MS))
		correction = median;
		
	PlayerChunk* playerChunk = getNextPlayerChunk(correction);
	int age = getAge(playerChunk) - bufferMs + outputBufferDacTime*1000;
	pBuffer->add(age);
	pShortBuffer->add(age);
//		std::cerr << "Chunk: " << age << "\t" << outputBufferDacTime*1000 << "\n";

	int sleep(0);
	time_t now = time(NULL);
	if (now != lastUpdate)
	{
		lastUpdate = now;
		median = pBuffer->median();
		shortMedian = pShortBuffer->median();
		if (abs(age) > 300)
			sleep = age;
		if (pShortBuffer->full() && (abs(shortMedian) > WIRE_CHUNK_MS))
			sleep = shortMedian;
		if (pBuffer->full() && (abs(median) > PLAYER_CHUNK_MS))
			sleep = median;
		std::cerr << "Chunk: " << age << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << outputBufferDacTime*1000 << "\n";
	}

	if (sleep != 0)
	{
		std::cerr << "Sleep: " << sleep << "\n";
		pBuffer->clear();
		pShortBuffer->clear();
		if (sleep < 0)
		{
			sleepMs(100-sleep);
		}
		else
		{
			for (size_t n=0; n<(size_t)(sleep / PLAYER_CHUNK_MS); ++n)
			{
				delete playerChunk;
				playerChunk = getNextPlayerChunk();
			}
		}
	}


//		int age = getAge(*lastPlayerChunk) + outputBufferDacTime*1000 - bufferMs;
	return playerChunk;
}


void Stream::sleepMs(int ms)
{
	if (ms > 0)
		usleep(ms * 1000);
}


