#include "stream.h"
#include <iostream>
#include <string.h>
#include <unistd.h>

using namespace std;

Stream::Stream() : sleep(0), median(0), shortMedian(0), lastUpdate(0)
{
	pBuffer = new DoubleBuffer<int>(15000 / PLAYER_CHUNK_MS);
	pShortBuffer = new DoubleBuffer<int>(5000 / PLAYER_CHUNK_MS);
	pMiniBuffer = new DoubleBuffer<int>(10);
	bufferMs = 500;
}



void Stream::setBufferLen(size_t bufferLenMs)
{
	bufferMs = bufferLenMs;
}



void Stream::addChunk(Chunk* chunk)
{
	chunks.push(shared_ptr<Chunk>(chunk));
}



void Stream::getSilentPlayerChunk(short* outputBuffer, unsigned long framesPerBuffer)
{
	memset(outputBuffer, 0, sizeof(short)*framesPerBuffer * CHANNELS);
}



time_point_ms Stream::getNextPlayerChunk(short* outputBuffer, unsigned long framesPerBuffer, int correction)
{
	if (!chunk)
		chunk = chunks.pop();

	time_point_ms tp = chunk->timePoint();
	int read = 0;
	int toRead = framesPerBuffer*CHANNELS + correction*PLAYER_CHUNK_MS_SIZE;
	short* buffer;

	if (correction != 0)
	{
		int msBuffer = floor(framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE);
		if (abs(correction) > msBuffer / 2)
			correction = copysign(msBuffer / 2, correction);
		buffer = (short*)malloc(toRead * sizeof(short));
	}
	else
		buffer = outputBuffer;

	while (read < toRead)
	{
		read += chunk->read(buffer + read, toRead - read);
		if (chunk->isEndOfChunk())
			chunk = chunks.pop();
	}

	if (correction != 0)
	{
		float factor = (float)toRead / (float)(framesPerBuffer*CHANNELS);
		std::cout << "correction: " << correction << ", factor: " << factor << "\n";
		for (size_t n=0; n<framesPerBuffer; ++n)
		{
			size_t index = (int)(floor(n*factor));
			*(outputBuffer + 2*n) = *(buffer + 2*index);
			*(outputBuffer + 2*n+1) = *(buffer + 2*index + 1);
		}
		free(buffer);
	}

	return tp;
}


void Stream::updateBuffers(int age)
{
	pBuffer->add(age);
	pMiniBuffer->add(age);
	pShortBuffer->add(age);
}


void Stream::resetBuffers()
{
	pBuffer->clear();
	pMiniBuffer->clear();
	pShortBuffer->clear();
}


void Stream::getPlayerChunk(short* outputBuffer, double outputBufferDacTime, unsigned long framesPerBuffer)
{
//cout << "framesPerBuffer: " << framesPerBuffer << "\tms: " << framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE << "\t" << PLAYER_CHUNK_SIZE << "\n";
	int msBuffer = floor(framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE);
	if (sleep != 0)
	{
		resetBuffers();
		if (sleep < 0)
		{
//			std::cerr << "Sleep: " << sleep << "\n";
			sleep += PLAYER_CHUNK_MS;
			if (sleep > -PLAYER_CHUNK_MS/2)
				sleep = 0;
			getSilentPlayerChunk(outputBuffer, framesPerBuffer);
		}
		else
		{
//			for (size_t i=0; i<chunks.size(); ++i)
//				std::cerr << "Chunk " << i << ": " << chunks[i]->getAge() - bufferMs << "\n";
			while (true)// (int i=0; i<(int)(round((float)sleep / (float)PLAYER_CHUNK_MS)) + 1; ++i)
			{
				int age = Chunk::getAge(getNextPlayerChunk(outputBuffer, framesPerBuffer)) - bufferMs;
				if (age < msBuffer / 2)
					break;
			}
			sleep = 0;
		}
		return;
	}
	
	int correction(0);
	if ((pBuffer->full() && (abs(median) <= msBuffer) && (abs(median) > 1)) ||
		(pShortBuffer->full() && (abs(shortMedian) <= msBuffer) && (abs(shortMedian) > max(7, msBuffer))))
	{
		correction = shortMedian;
		resetBuffers();
	} 
//correction = 0;
//if (time(NULL) != lastUpdate)
//	correction = 2;

	int age = Chunk::getAge(getNextPlayerChunk(outputBuffer, framesPerBuffer, correction)) - bufferMs;// + outputBufferDacTime*1000;
//	cout << age << "\t";
	if (outputBufferDacTime < 1)
		age += outputBufferDacTime*1000;
//	cout << age << "\t" << outputBufferDacTime*1000 << "\n";

	if (pShortBuffer->full() && (abs(shortMedian) > max(15, msBuffer)))
	{
		sleep = shortMedian;
		std::cerr << "Sleep: " << sleep << "\n";
	}
	else if (pMiniBuffer->full() && (abs(age) > 50) && (abs(pMiniBuffer->mean()) > 50))
	{
		sleep = pMiniBuffer->mean();
		std::cerr << "Sleep: " << sleep << "\n";
	}


//	std::cerr << "Chunk: " << age << "\t" << outputBufferDacTime*1000 << "\n";
	updateBuffers(age);
	time_t now = time(NULL);
	if (now != lastUpdate)
	{
		lastUpdate = now;
		median = pBuffer->mean();
		shortMedian = pShortBuffer->mean();
		std::cerr << "Chunk: " << age << "\t" << pMiniBuffer->mean() << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << outputBufferDacTime*1000 << "\n";
	}
}



