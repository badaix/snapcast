#include "stream.h"
#include <iostream>
#include <string.h>
#include <unistd.h>

Stream::Stream() : sleep(0), median(0), shortMedian(0), lastUpdate(0), currentSample(0), everyN(40000)
{
	pBuffer = new DoubleBuffer<int>(15000 / PLAYER_CHUNK_MS);
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



time_point_ms Stream::getNextPlayerChunk(short* outputBuffer, int correction)
{
	Chunk* chunk = getNextChunk();
	time_point_ms tp = timePoint(chunk);

	if (correction != 0)
	{
		float idx(chunk->idx);
		float factor = 2*(1.f - (float)(correction) / (float)(PLAYER_CHUNK_MS));
		for (size_t n=0; n<PLAYER_CHUNK_SIZE; n+=2)
		{
			size_t index = (int)(floor(idx));
			*(outputBuffer + n) = chunk->payload[index];
			*(outputBuffer + n+1) = chunk->payload[index + 1];
			idx += factor;
			if (idx >= WIRE_CHUNK_SIZE)
			{
				chunks.pop_front();
				delete chunk;
				chunk = getNextChunk();
				idx -= WIRE_CHUNK_SIZE;
			}
		}
		chunk->idx = idx;
//		if (correction != 0)
//			std::cerr << "Diff: " << diff_ms(getTimeval(chunk), tv) << "\t" << chunk->idx / PLAYER_CHUNK_MS_SIZE << "\n";
	}
	else
	{
/*		int idx(chunk->idx);
		for (size_t n=0; n<PLAYER_CHUNK_SIZE; n+=2)
		{
			*(outputBuffer + n) = chunk->payload[idx];
			*(outputBuffer + n+1) = chunk->payload[idx + 1];
			idx += 2;
			if (++currentSample > everyN)
			{
				idx += 2;
				currentSample = 0;
			}
			if (idx >= WIRE_CHUNK_SIZE)
			{
				chunks.pop_front();
				delete chunk;
				chunk = getNextChunk();
				idx -= WIRE_CHUNK_SIZE;
			}
		}
		chunk->idx = idx;
*/
		size_t missing = PLAYER_CHUNK_SIZE;// + correction*PLAYER_CHUNK_MS_SIZE;
		if (chunk->idx + PLAYER_CHUNK_SIZE > WIRE_CHUNK_SIZE)
		{
			if (outputBuffer != NULL)
				memcpy(outputBuffer, &chunk->payload[chunk->idx], sizeof(int16_t)*(WIRE_CHUNK_SIZE - chunk->idx));
			missing = chunk->idx + PLAYER_CHUNK_SIZE - WIRE_CHUNK_SIZE;
			chunks.pop_front();
			delete chunk;
			chunk = getNextChunk();
		}

		if (outputBuffer != NULL)
			memcpy((outputBuffer + PLAYER_CHUNK_SIZE - missing), &chunk->payload[chunk->idx], sizeof(int16_t)*missing);

		chunk->idx += missing;
		if (chunk->idx >= WIRE_CHUNK_SIZE)
		{
			chunks.pop_front();
			delete chunk;
		}

	}

	return tp;
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
			if (sleep > -PLAYER_CHUNK_MS/2)
				sleep = 0;
			getSilentPlayerChunk(outputBuffer);
		}
		else
		{
			for (size_t i=0; i<chunks.size(); ++i)
				std::cerr << "Chunk " << i << ": " << getAge(chunks[i]) - bufferMs << "\n";
			while (true)// (int i=0; i<(int)(round((float)sleep / (float)PLAYER_CHUNK_MS)) + 1; ++i)
			{
//				std::cerr << "Sleep: " << sleep << "\n";
				int age = getAge(getNextPlayerChunk(outputBuffer)) - bufferMs;
				if (age < PLAYER_CHUNK_MS / 2)
					break;
//				std::cerr << getAge(getNextPlayerChunk(outputBuffer)) - bufferMs << "\t";
//				usleep(10);
			}
			sleep = 0;
		}
		return;
	}
	
	int correction(0);
	if (pBuffer->full() && (abs(median) <= PLAYER_CHUNK_MS))
	{
		if (abs(median) > 1)
		{
			correction = -shortMedian;
			pBuffer->clear();
			pShortBuffer->clear();
		}
	} 
	else if (pShortBuffer->full() && (abs(shortMedian) <= PLAYER_CHUNK_MS))
	{
		if (abs(shortMedian) > 3)
		{
			correction = -shortMedian;
			pBuffer->clear();
			pShortBuffer->clear();
		}
	}	

	int age = getAge(getNextPlayerChunk(outputBuffer, correction)) - bufferMs;// + outputBufferDacTime*1000;
	if (outputBufferDacTime < 1)
		age += outputBufferDacTime*1000;
	pBuffer->add(age);
	pShortBuffer->add(age);
//	std::cerr << "Chunk: " << age << "\t" << outputBufferDacTime*1000 << "\n";

	time_t now = time(NULL);
	if (now != lastUpdate)
	{
		lastUpdate = now;
		median = pBuffer->median();
		shortMedian = pShortBuffer->median();
		if (abs(age) > 100)
			sleep = age;
		else if (pShortBuffer->full() && (abs(shortMedian) > PLAYER_CHUNK_MS))
			sleep = shortMedian;

		if (sleep != 0)
		{
			std::cerr << "Sleep: " << sleep << "\n";
		}
//sleep = 0;
		std::cerr << "Chunk: " << age << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << outputBufferDacTime*1000 << "\n";
	}
}


void Stream::sleepMs(int ms)
{
	if (ms > 0)
		usleep(ms * 1000);
}


