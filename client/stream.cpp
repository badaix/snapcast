#include "stream.h"
#include <iostream>
#include <string.h>
#include <unistd.h>
#include "common/log.h"
#include "timeProvider.h"

using namespace std;
using namespace chronos;


Stream::Stream(const SampleFormat& sampleFormat) : format(format_), format_(sampleFormat), sleep(0), median(0), shortMedian(0), lastUpdate(0)
{
	buffer.setSize(500);
	shortBuffer.setSize(100);
	miniBuffer.setSize(20);
	cardBuffer.setSize(50);
	bufferMs = msec(500);
}



void Stream::setBufferLen(size_t bufferLenMs)
{
	bufferMs = msec(bufferLenMs);
}



void Stream::clearChunks()
{
	while (chunks.size() > 0)
		chunks.pop();
}


void Stream::addChunk(PcmChunk* chunk)
{
	while (chunks.size() * chunk->duration<chronos::msec>().count() > 10000)
		chunks.pop();
	chunks.push(shared_ptr<PcmChunk>(chunk));
//	cout << "new chunk: " << chunk->getDuration() << ", Chunks: " << chunks.size() << "\n";
}



time_point_hrc Stream::getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer)
{
	if (!chunk)
		chunk = chunks.pop();
	time_point_hrc tp = chunk->start();
	memset(outputBuffer, 0, framesPerBuffer * format.frameSize);
	return tp;
}


/*
time_point_ms Stream::seekTo(const time_point_ms& to)
{
	if (!chunk)
		chunk = chunks.pop();
//	time_point_ms tp = chunk->timePoint();
	while (to > chunk->timePoint())// + std::chrono::milliseconds((long int)chunk->getDuration()))//
	{
		chunk = chunks.pop();
		cout << "\nto: " << Chunk::getAge(to) << "\t chunk: " << Chunk::getAge(chunk->timePoint()) << "\n";
		cout << "diff: " << std::chrono::duration_cast<std::chrono::milliseconds>((to - chunk->timePoint())).count() << "\n";
	}
	chunk->seek(std::chrono::duration_cast<std::chrono::milliseconds>(to - chunk->timePoint()).count() * format.msRate());
	return chunk->timePoint();
}
*/


time_point_hrc Stream::seek(long ms)
{
	if (!chunk)
		chunk = chunks.pop();

	if (ms <= 0)
		return chunk->start();

//	time_point_ms tp = chunk->timePoint();
	while (ms > chunk->duration<chronos::msec>().count())
	{
		chunk = chunks.pop();
		ms -= min(ms, (long)chunk->durationLeft<chronos::msec>().count());
	}
	chunk->seek(ms * format.msRate());
	return chunk->start();
}


time_point_hrc Stream::getNextPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer, size_t timeout, const chronos::usec& correction)
{
	if (!chunk)
		if (!chunks.try_pop(chunk, chronos::msec(timeout)))
			throw 0;

//cout << "duration: " << chunk->duration<chronos::msec>().count() << ", " << chunk->duration<chronos::usec>().count() << ", " << chunk->duration<chronos::nsec>().count() << "\n";
	time_point_hrc tp = chunk->start();
	int read = 0;
	int toRead = framesPerBuffer + correction.count()*format.usRate();
	char* buffer;

	if (correction.count() != 0)
	{
//		chronos::usec usBuffer = (chronos::usec::rep)(framesPerBuffer / format.usRate());
//		if (abs(correction) > usBuffer / 2)
//			correction = copysign(usBuffer / 2, correction);
		buffer = (char*)malloc(toRead * format.frameSize);
	}
	else
		buffer = (char*)outputBuffer;

	while (read < toRead)
	{
		read += chunk->readFrames(buffer + read*format.frameSize, toRead - read);
		if (chunk->isEndOfChunk())
			if (!chunks.try_pop(chunk, std::chrono::milliseconds(timeout)))
				throw 0;
	}

	if (correction.count() != 0)
	{
		float factor = (float)toRead / framesPerBuffer;//(float)(framesPerBuffer*channels_);
		std::cout << "correction: " << correction.count() << ", factor: " << factor << "\n";
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
	buffer.add(age);
	miniBuffer.add(age);
	shortBuffer.add(age);
}


void Stream::resetBuffers()
{
	buffer.clear();
	miniBuffer.clear();
	shortBuffer.clear();
}



bool Stream::getPlayerChunk(void* outputBuffer, chronos::usec outputBufferDacTime, unsigned long framesPerBuffer, size_t timeout)
{
	try
	{
	//cout << "framesPerBuffer: " << framesPerBuffer << "\tms: " << framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE << "\t" << PLAYER_CHUNK_SIZE << "\n";
		chronos::nsec bufferDuration = chronos::nsec(chronos::usec::rep(framesPerBuffer / format_.nsRate()));
//		cout << "buffer duration: " << bufferDuration.count() << "\n";

		chronos::usec correction = chronos::usec(0);
		if (sleep.count() != 0)
		{
//			cout << "Sleep " << sleep.count() << "\n";
			resetBuffers();
			if (sleep < -bufferDuration/2)
			{
				usec age = chrono::duration_cast<usec>(TimeProvider::serverNow() - getSilentPlayerChunk(outputBuffer, framesPerBuffer) - bufferMs + outputBufferDacTime);
				sleep = age;
//				sleep = PcmChunk::getAge(getSilentPlayerChunk(outputBuffer, framesPerBuffer)) - bufferMs + outputBufferDacTime + TimeProvider::getInstance().getDiffToServerMs();
				std::cerr << " after: " << sleep.count() << ", chunks: " << chunks.size() << "\n";
	//	std::clog << kLogNotice << "sleep: " << sleep << std::endl;
	//			if (sleep > -msBuffer/2)
	//				sleep = 0;
				if (sleep < -bufferDuration/2)
					return true;
			}
			else if (sleep > bufferDuration/2)
			{
				if (!chunk)
					if (!chunks.try_pop(chunk, chronos::msec(timeout)))
						throw 0;
				while (sleep > chunk->duration<chronos::msec>())
				{
					cout << "sleep > chunk->getDuration(): " << sleep.count() << " > " << chunk->duration<chronos::msec>().count() << ", chunks: " << chunks.size() << ", out: " << outputBufferDacTime.count() << ", needed: " << bufferDuration.count() << "\n";
					if (!chunks.try_pop(chunk, chronos::msec(timeout)))
						throw 0;
					msec age = std::chrono::duration_cast<msec>(TimeProvider::serverNow() - chunk->start() - bufferMs + outputBufferDacTime);
					sleep = age;
					usleep(1000);
				}
//				cout << "seek: " << PcmChunk::getAge(seek(sleep)) - bufferMs + outputBufferDacTime << "\n";
				sleep = chronos::usec(0);
			}
			else if (sleep < -chronos::usec(100))
			{
				sleep += chronos::usec(100);
				correction = -chronos::usec(100);
			}
			else if (sleep > chronos::usec(100))
			{
				sleep -= chronos::usec(100);
				correction = chronos::usec(100);
			}
			else
			{
				cout << "Sleep " << sleep.count() << "\n";
				correction = sleep;
				sleep = chronos::usec(0);
			}
		}

		chronos::usec age = std::chrono::duration_cast<usec>(TimeProvider::serverNow() - getNextPlayerChunk(outputBuffer, framesPerBuffer, timeout, correction) - bufferMs + outputBufferDacTime);

		if (sleep.count() == 0)
		{
			if (buffer.full() && (chronos::usec(abs(median)) > chronos::msec(1)))
			{
				cout << "pBuffer->full() && (abs(median) > 1): " << median << "\n";
				sleep = chronos::usec(median);
			}
			else if (shortBuffer.full() && (chronos::usec(abs(shortMedian)) > chronos::msec(5)))
			{
				cout << "pShortBuffer->full() && (abs(shortMedian) > 5): " << shortMedian << "\n";
				sleep = chronos::usec(shortMedian);
			}
			else if (miniBuffer.full() && (chronos::usec(abs(miniBuffer.median())) > chronos::msec(50)))
			{
				cout << "pMiniBuffer->full() && (abs(pMiniBuffer->mean()) > 50): " << miniBuffer.median() << "\n";
				sleep = chronos::usec((chronos::msec::rep)miniBuffer.mean());
			}
			else if (chronos::abs(age) > chronos::msec(200))
			{
				cout << "age > 50: " << age.count() << "\n";
				sleep = age;
			}
		}

		if (sleep.count() != 0)
			std::cerr << "Sleep: " << sleep.count() << "\n";


		updateBuffers(age.count());

	//	std::cerr << "Chunk: " << age << "\t" << outputBufferDacTime*1000 << "\n";
		time_t now = time(NULL);
		if (now != lastUpdate)
		{
			lastUpdate = now;
			median = buffer.median();
			shortMedian = shortBuffer.median();
			std::cerr << "Chunk: " << age.count()/100 << "\t" << miniBuffer.median()/100 << "\t" << shortMedian/100 << "\t" << median/100 << "\t" << buffer.size() << "\t" << /*cardBuffer << "\t" <<*/ outputBufferDacTime.count() << "\n";
		}
		return true;
	}
	catch(int e)
	{
		sleep = chronos::usec(0);
		return false;
	}
}



