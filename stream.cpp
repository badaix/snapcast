#include "stream.h"
#include <iostream>
#include <string.h>
#include <unistd.h>

using namespace std;

Stream::Stream() : sleep(0), median(0), shortMedian(0), lastUpdate(0)
{
	pBuffer = new DoubleBuffer<int>(1000);
	pShortBuffer = new DoubleBuffer<int>(200);
	pMiniBuffer = new DoubleBuffer<int>(10);
	pCardBuffer = new DoubleBuffer<int>(50);
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

	int ticks = 0;
	long currentTick = getTickCount();
	if (lastTick == 0)
		lastTick = currentTick;
	ticks = currentTick - lastTick;
	lastTick = currentTick;

	int cardBuffer = 0;
	if (ticks > 1)
		pCardBuffer->add(ticks);
	if (pCardBuffer->full())
		cardBuffer = pCardBuffer->percentil(90);
		
	int correction = 0;
	if (sleep != 0)
	{
		resetBuffers();
		if (sleep < -10)
		{
//			std::cerr << "Sleep: " << sleep << "\n";
			sleep += msBuffer;
			if (sleep > -msBuffer/2)
				sleep = 0;
			getSilentPlayerChunk(outputBuffer, framesPerBuffer);
			return;
		}
		else if (sleep > 10)
		{
//			for (size_t i=0; i<chunks.size(); ++i)
//				std::cerr << "Chunk " << i << ": " << chunks[i]->getAge() - bufferMs << "\n";
			while (true)// (int i=0; i<(int)(round((float)sleep / (float)PLAYER_CHUNK_MS)) + 1; ++i)
			{
				int age = Chunk::getAge(getNextPlayerChunk(outputBuffer, framesPerBuffer)) - bufferMs;
		age += 2*cardBuffer;
				if (age < msBuffer / 2)
					break;
			}
			sleep = 0;
			return;
		}
		else if (sleep < 0)
		{	
			++sleep;
			correction = -1;
		}
		else if (sleep > 0)
		{	
			--sleep;
			correction = 1;
		}
	}
	


	int age = Chunk::getAge(getNextPlayerChunk(outputBuffer, framesPerBuffer, correction)) - bufferMs;// + outputBufferDacTime*1000;


	if (outputBufferDacTime < 1)
		age += outputBufferDacTime*1000;

//	if (pCardBuffer->full())
		age += 2*cardBuffer;

	cout << age << "\t" << msBuffer << "\t" << ticks << "\tcardBuffer: " << cardBuffer << "\n";
//	cout << age << "\t" << outputBufferDacTime*1000 << "\n";


	if (sleep == 0)
	{
		if (pBuffer->full() && (abs(median) <= 10) && (abs(median) > 1))
		{
			sleep = median;
		} 
		else if (pShortBuffer->full() && (abs(shortMedian) <= 10) && (abs(shortMedian) > 5))
		{
			sleep = shortMedian;
		} 
		if (pShortBuffer->full() && (abs(shortMedian) > 10))
		{
			sleep = shortMedian;
		}
		else if (pMiniBuffer->full() && (abs(age) > 50) && (abs(pMiniBuffer->mean()) > 50))
		{
			sleep = pMiniBuffer->mean();
		}
	}

	if (sleep != 0)
		std::cerr << "Sleep: " << sleep << "\n";
	

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



