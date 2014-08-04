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
//	Chunk* c = new Chunk(*chunk);
//	mutex.lock();
	chunks.push_back(chunk);
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
	time_point_ms tp = chunk->timePoint();
	int read = 0;
	int toRead = PLAYER_CHUNK_SIZE + correction*PLAYER_CHUNK_MS_SIZE;
	short* buffer;

	if (correction != 0)
		buffer = (short*)malloc(toRead * sizeof(short));
	else
		buffer = outputBuffer;

	while (read < toRead)
	{
		read += chunk->read(buffer + read, toRead - read);
		if (chunk->isEndOfChunk())
		{
			chunks.pop_front();
			delete chunk;
			chunk = getNextChunk();
		}
	}

	if (correction != 0)
	{
		float factor = (float)toRead / (float)PLAYER_CHUNK_SIZE;
		std::cout << "correction: " << correction << ", factor: " << factor << "\n";
		for (size_t n=0; n<PLAYER_CHUNK_SIZE/2; ++n)
		{
			size_t index = (int)(floor(n*factor));
			*(outputBuffer + 2*n) = *(buffer + 2*index);
			*(outputBuffer + 2*n+1) = *(buffer + 2*index + 1);
		}
		free(buffer);
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
			std::cerr << "Sleep: " << sleep << "\n";
			sleep += PLAYER_CHUNK_MS;
			if (sleep > -PLAYER_CHUNK_MS/2)
				sleep = 0;
			getSilentPlayerChunk(outputBuffer);
		}
		else
		{
			for (size_t i=0; i<chunks.size(); ++i)
				std::cerr << "Chunk " << i << ": " << chunks[i]->getAge() - bufferMs << "\n";
			while (true)// (int i=0; i<(int)(round((float)sleep / (float)PLAYER_CHUNK_MS)) + 1; ++i)
			{
//				std::cerr << "Sleep: " << sleep << "\n";
				int age = Chunk::getAge(getNextPlayerChunk(outputBuffer)) - bufferMs;
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
			correction = shortMedian;
			pBuffer->clear();
			pShortBuffer->clear();
		}
	} 
	else if (pShortBuffer->full() && (abs(shortMedian) <= PLAYER_CHUNK_MS))
	{
		if (abs(shortMedian) > 3)
		{
			correction = shortMedian;
			pBuffer->clear();
			pShortBuffer->clear();
		}
	}

	int age = Chunk::getAge(getNextPlayerChunk(outputBuffer, correction)) - bufferMs;// + outputBufferDacTime*1000;
	if (outputBufferDacTime < 1)
		age += outputBufferDacTime*1000;
//	std::cerr << "Chunk: " << age << "\t" << outputBufferDacTime*1000 << "\n";
	pBuffer->add(age);
	pShortBuffer->add(age);

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


