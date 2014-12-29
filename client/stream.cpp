#include "stream.h"
#include <iostream>
#include <string.h>
#include "common/log.h"
#include "timeProvider.h"

using namespace std;
using namespace chronos;


Stream::Stream(const msg::SampleFormat& sampleFormat) : format(format_), format_(sampleFormat), sleep(0), median(0), shortMedian(0), lastUpdate(0), playedFrames(0)
{
	buffer.setSize(500);
	shortBuffer.setSize(100);
	miniBuffer.setSize(20);
//	cardBuffer.setSize(50);
	bufferMs = msec(500);

/*
48000     x
------- = -----
47999,2   x - 1

x = 1,000016667 / (1,000016667 - 1)
*/
	setRealSampleRate(format.rate);
}


void Stream::setRealSampleRate(double sampleRate)
{
	if (sampleRate == format.rate)
		correctAfterXFrames = 0;
	else
		correctAfterXFrames = round((format.rate / sampleRate) / (format.rate / sampleRate - 1.));
//	logD << "Correct after X: " << correctAfterXFrames << " (Real rate: " << sampleRate << ", rate: " << format.rate << ")\n";
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


void Stream::addChunk(msg::PcmChunk* chunk)
{
	while (chunks.size() * chunk->duration<chronos::msec>().count() > 10000)
		chunks.pop();
	chunks.push(shared_ptr<msg::PcmChunk>(chunk));
//	logD << "new chunk: " << chunk->getDuration() << ", Chunks: " << chunks.size() << "\n";
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
		logD << "\nto: " << Chunk::getAge(to) << "\t chunk: " << Chunk::getAge(chunk->timePoint()) << "\n";
		logD << "diff: " << std::chrono::duration_cast<std::chrono::milliseconds>((to - chunk->timePoint())).count() << "\n";
	}
	chunk->seek(std::chrono::duration_cast<std::chrono::milliseconds>(to - chunk->timePoint()).count() * format.msRate());
	return chunk->timePoint();
}
*/

/*
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
*/


time_point_hrc Stream::getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer)
{
	if (!chunk && !chunks.try_pop(chunk, timeout))
		throw 0;

	time_point_hrc tp = chunk->start();
	char* buffer = (char*)outputBuffer;
	unsigned long read = 0;
	while (read < framesPerBuffer)
	{
		read += chunk->readFrames(buffer + read*format.frameSize, framesPerBuffer - read);
		if (chunk->isEndOfChunk() && !chunks.try_pop(chunk, timeout))
			throw 0;
	}
	return tp;
}


time_point_hrc Stream::getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer, long framesCorrection)
{
	if (framesCorrection == 0)
		return getNextPlayerChunk(outputBuffer, timeout, framesPerBuffer);

	long toRead = framesPerBuffer + framesCorrection;
	char* buffer = (char*)malloc(toRead * format.frameSize);
	time_point_hrc tp = getNextPlayerChunk(buffer, timeout, toRead);

	float factor = (float)toRead / framesPerBuffer;//(float)(framesPerBuffer*channels_);
	if (abs(framesCorrection) > 1)
		logO << "correction: " << framesCorrection << ", factor: " << factor << "\n";
	float idx = 0;
	for (size_t n=0; n<framesPerBuffer; ++n)
	{
		size_t index(floor(idx));// = (int)(ceil(n*factor));
		memcpy((char*)outputBuffer + n*format.frameSize, buffer + index*format.frameSize, format.frameSize);
		idx += factor;
	}
	free(buffer);

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



bool Stream::getPlayerChunk(void* outputBuffer, const chronos::usec& outputBufferDacTime, unsigned long framesPerBuffer)
{
	if (outputBufferDacTime > bufferMs)
		return false;

	if (!chunk && !chunks.try_pop(chunk, outputBufferDacTime))
		return false;

	playedFrames += framesPerBuffer;

	chronos::usec age = std::chrono::duration_cast<usec>(TimeProvider::serverNow() - chunk->start() - bufferMs + outputBufferDacTime);
	if ((sleep.count() == 0) && (chronos::abs(age) > chronos::msec(200)))
	{
		logO << "age > 200: " << age.count() << "\n";
		sleep = age;
	}

	try
	{

	//logD << "framesPerBuffer: " << framesPerBuffer << "\tms: " << framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE << "\t" << PLAYER_CHUNK_SIZE << "\n";
		chronos::nsec bufferDuration = chronos::nsec(chronos::usec::rep(framesPerBuffer / format_.nsRate()));
//		logD << "buffer duration: " << bufferDuration.count() << "\n";

		chronos::usec correction = chronos::usec(0);
		if (sleep.count() != 0)
		{
			resetBuffers();
			if (sleep < -bufferDuration/2)
			{
				// We're early: not enough chunks. play silence. Reference chunk is the oldest (front) one
				sleep = chrono::duration_cast<usec>(TimeProvider::serverNow() - getSilentPlayerChunk(outputBuffer, framesPerBuffer) - bufferMs + outputBufferDacTime);
//logD << "-sleep: " << sleep.count() << " " << -bufferDuration.count() / 2000 << "\n";
				if (sleep < -bufferDuration/2)
					return true;
			}
			else if (sleep > bufferDuration/2)
			{
				// We're late: discard oldest chunks
				while (sleep > chunk->duration<chronos::usec>())
				{
					logO << "sleep > chunk->getDuration(): " << sleep.count() << " > " << chunk->duration<chronos::msec>().count() << ", chunks: " << chunks.size() << ", out: " << outputBufferDacTime.count() << ", needed: " << bufferDuration.count() << "\n";
					sleep = std::chrono::duration_cast<usec>(TimeProvider::serverNow() - chunk->start() - bufferMs + outputBufferDacTime);
					if (!chunks.try_pop(chunk, outputBufferDacTime))
					{
						logO << "no chunks available\n";
						chunk = NULL;
						sleep = chronos::usec(0);
						return false;
					}
				}
			}

			// out of sync, can be corrected by playing faster/slower
			if (sleep < -chronos::usec(100))
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
				logO << "Sleep " << sleep.count() << "\n";
				correction = sleep;
				sleep = chronos::usec(0);
			}
		}

		long framesCorrection = correction.count()*format.usRate();
		if ((correctAfterXFrames != 0) && (playedFrames >= (unsigned long)abs(correctAfterXFrames)))
		{
			framesCorrection += (correctAfterXFrames > 0)?1:-1;
			playedFrames -= abs(correctAfterXFrames);
		}

		age = std::chrono::duration_cast<usec>(TimeProvider::serverNow() - getNextPlayerChunk(outputBuffer, outputBufferDacTime, framesPerBuffer, framesCorrection) - bufferMs + outputBufferDacTime);

		setRealSampleRate(format.rate);
		if (sleep.count() == 0)
		{
			if (buffer.full())
			{
				if (chronos::usec(abs(median)) > chronos::msec(1))
				{
					logO << "pBuffer->full() && (abs(median) > 1): " << median << "\n";
					sleep = chronos::usec(shortMedian);
				}
/*				else if (chronos::usec(median) > chronos::usec(300))
				{
					setRealSampleRate(format.rate - format.rate / 1000);
				}
				else if (chronos::usec(median) < -chronos::usec(300))
				{
					setRealSampleRate(format.rate + format.rate / 1000);
				}
*/			}
			else if (shortBuffer.full())
			{
				if (chronos::usec(abs(shortMedian)) > chronos::msec(5))
				{
					logO << "pShortBuffer->full() && (abs(shortMedian) > 5): " << shortMedian << "\n";
					sleep = chronos::usec(shortMedian);
				}
/*				else
				{
					setRealSampleRate(format.rate + -shortMedian / 100);
				}
*/			}
			else if (miniBuffer.full() && (chronos::usec(abs(miniBuffer.median())) > chronos::msec(50)))
			{
				logO << "pMiniBuffer->full() && (abs(pMiniBuffer->mean()) > 50): " << miniBuffer.median() << "\n";
				sleep = chronos::usec((chronos::msec::rep)miniBuffer.mean());
			}
		}

		if (sleep.count() != 0)
			logO << "Sleep: " << sleep.count() << "\n";
		else if (shortBuffer.full())
		{
			if (chronos::usec(shortMedian) > chronos::usec(100))
				setRealSampleRate(format.rate * 0.9999);
			else if (chronos::usec(shortMedian) < -chronos::usec(100))
				setRealSampleRate(format.rate * 1.0001);
		}

		updateBuffers(age.count());

		// print sync stats
		time_t now = time(NULL);
		if (now != lastUpdate)
		{
			lastUpdate = now;
			median = buffer.median();
			shortMedian = shortBuffer.median();
			logO << "Chunk: " << age.count()/100 << "\t" << miniBuffer.median()/100 << "\t" << shortMedian/100 << "\t" << median/100 << "\t" << buffer.size() << "\t" << outputBufferDacTime.count() << "\n";
		}
		return true;
	}
	catch(int e)
	{
		sleep = chronos::usec(0);
		return false;
	}
}



