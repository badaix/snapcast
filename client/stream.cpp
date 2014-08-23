#include "stream.h"
#include <iostream>
#include <string.h>
#include <unistd.h>
#include "common/log.h"

using namespace std;

Stream::Stream(const SampleFormat& sampleFormat) : format(format_), format_(sampleFormat), sleep(0), median(0), shortMedian(0), lastUpdate(0)
{
	pBuffer = new DoubleBuffer<int>(1000);
	pShortBuffer = new DoubleBuffer<int>(200);
	pMiniBuffer = new DoubleBuffer<int>(20);
	pCardBuffer = new DoubleBuffer<int>(50);
	bufferMs = 500;
}



void Stream::setBufferLen(size_t bufferLenMs)
{
	bufferMs = bufferLenMs;
}



void Stream::clearChunks()
{
	while (chunks.size() > 0)
		chunks.pop();
}


void Stream::addChunk(Chunk* chunk)
{
	while (chunks.size() * chunk->getDuration() > 10000)
		chunks.pop();
//	cout << "new chunk: " << chunk->getDuration() << ", Chunks: " << chunks.size() << "\n";
	chunks.push(shared_ptr<Chunk>(chunk));
}



time_point_ms Stream::getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer)
{
	if (!chunk)
		chunk = chunks.pop();
	time_point_ms tp = chunk->timePoint();
	memset(outputBuffer, 0, framesPerBuffer * format.frameSize);
	return tp;
}



time_point_ms Stream::getNextPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer, int correction)
{
	if (!chunk)
		chunk = chunks.pop();

	time_point_ms tp = chunk->timePoint();
	int read = 0;
	int toRead = framesPerBuffer + correction*format.msRate();
	char* buffer;

	if (correction != 0)
	{
		int msBuffer = floor(framesPerBuffer / format.msRate());
		if (abs(correction) > msBuffer / 2)
			correction = copysign(msBuffer / 2, correction);
		buffer = (char*)malloc(toRead * format.frameSize);
	}
	else
		buffer = (char*)outputBuffer;

	while (read < toRead)
	{
		read += chunk->read(buffer + read*format.frameSize, toRead - read);
		if (chunk->isEndOfChunk())
			chunk = chunks.pop();
	}

	if (correction != 0)
	{
		float factor = (float)toRead / framesPerBuffer;//(float)(framesPerBuffer*channels_);
		std::cout << "correction: " << correction << ", factor: " << factor << "\n";
		float idx = 0;
		for (size_t n=0; n<framesPerBuffer; ++n)
		{
			size_t index(floor(idx));// = (int)(ceil(n*factor));
			memcpy((char*)outputBuffer + n*format.frameSize, buffer + index*format.frameSize, format.frameSize);
			idx += factor;
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


void Stream::getPlayerChunk(void* outputBuffer, double outputBufferDacTime, unsigned long framesPerBuffer)
{
//cout << "framesPerBuffer: " << framesPerBuffer << "\tms: " << framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE << "\t" << PLAYER_CHUNK_SIZE << "\n";
//int msBuffer = framesPerBuffer / (format_.rate/1000);
//cout << msBuffer << " ms, " << framesPerBuffer << "\t" << hz_/1000 << "\n";
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
			sleep = Chunk::getAge(getSilentPlayerChunk(outputBuffer, framesPerBuffer)) - bufferMs + outputBufferDacTime;
			std::cerr << "Sleep: " << sleep << ", chunks: " << chunks.size() << "\n";
//	std::clog << kLogNotice << "sleep: " << sleep << std::endl;
//			if (sleep > -msBuffer/2)
//				sleep = 0;
			return;
		}
		else if (sleep > 10)
		{
//			std::clog << kLogNotice << "sleep: " << sleep << std::endl;
			while (true)
			{
				sleep = Chunk::getAge(getNextPlayerChunk(outputBuffer, framesPerBuffer)) - bufferMs + outputBufferDacTime;
				usleep(100);
//				std::clog << kLogNotice << "age: " << age << std::endl;
				if (sleep < 0)
					break;
			}
//			sleep = 0;
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
	


	int age = Chunk::getAge(getNextPlayerChunk(outputBuffer, framesPerBuffer, correction)) - bufferMs + outputBufferDacTime;


//	if (pCardBuffer->full())
//		age += 4*cardBuffer;

//	cout << age << "\t" << framesPerBuffer << "\t" << msBuffer << "\t" << ticks << "\t" << cardBuffer << "\t" << outputBufferDacTime*1000 << "\n";


	if (sleep == 0)
	{
		if (pBuffer->full() && (abs(median) > 1))
		{
			cout << "pBuffer->full() && (abs(median) > 1): " << median << "\n";
			sleep = median;
		} 
		else if (pShortBuffer->full() && (abs(shortMedian) > 5))
		{
			cout << "pShortBuffer->full() && (abs(shortMedian) > 5): " << shortMedian << "\n";
			sleep = shortMedian;
		} 
		else if (pMiniBuffer->full() && (abs(pMiniBuffer->median()) > 50))
		{
			cout << "pMiniBuffer->full() && (abs(pMiniBuffer->mean()) > 50): " << pMiniBuffer->median() << "\n";
			sleep = pMiniBuffer->mean();
		}
	}

	if (sleep != 0)
		std::cerr << "Sleep: " << sleep << "\n";
	

//	std::cerr << "Chunk: " << age << "\t" << outputBufferDacTime*1000 << "\n";
	if (ticks > 2)
	{
//		cout << age << "\n";
		updateBuffers(age);
	}
	time_t now = time(NULL);
	if (now != lastUpdate)
	{
		lastUpdate = now;
		median = pBuffer->median();
		shortMedian = pShortBuffer->median();
		std::cerr << "Chunk: " << age << "\t" << pMiniBuffer->median() << "\t" << shortMedian << "\t" << median << "\t" << pBuffer->size() << "\t" << cardBuffer << "\t" << outputBufferDacTime << "\n";
	}
}



