#include "stream.h"
#include <iostream>
#include <string.h>
#include "common/log.h"
#include "timeProvider.h"

using namespace std;
using namespace chronos;


Stream::Stream(const msg::SampleFormat& sampleFormat) : format_(sampleFormat), sleep_(0), median_(0), shortMedian_(0), lastUpdate_(0), playedFrames_(0)
{
	buffer_.setSize(500);
	shortBuffer_.setSize(100);
	miniBuffer_.setSize(20);
//	cardBuffer_.setSize(50);
	bufferMs_ = msec(500);

/*
48000     x
------- = -----
47999,2   x - 1

x = 1,000016667 / (1,000016667 - 1)
*/
	setRealSampleRate(format_.rate);
}


void Stream::setRealSampleRate(double sampleRate)
{
	if (sampleRate == format_.rate)
		correctAfterXFrames_ = 0;
	else
		correctAfterXFrames_ = round((format_.rate / sampleRate) / (format_.rate / sampleRate - 1.));
//	logD << "Correct after X: " << correctAfterXFrames_ << " (Real rate: " << sampleRate << ", rate: " << format_.rate << ")\n";
}


void Stream::setBufferLen(size_t bufferLenMs)
{
	bufferMs_ = msec(bufferLenMs);
}



void Stream::clearChunks()
{
	while (chunks_.size() > 0)
		chunks_.pop();
}


void Stream::addChunk(msg::PcmChunk* chunk)
{
	while (chunks_.size() * chunk->duration<chronos::msec>().count() > 10000)
		chunks_.pop();
	chunks_.push(shared_ptr<msg::PcmChunk>(chunk));
//	logD << "new chunk: " << chunk_->getDuration() << ", Chunks: " << chunks_.size() << "\n";
}



time_point_hrc Stream::getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer)
{
	if (!chunk_)
		chunk_ = chunks_.pop();
	time_point_hrc tp = chunk_->start();
	memset(outputBuffer, 0, framesPerBuffer * format_.frameSize);
	return tp;
}


/*
time_point_ms Stream::seekTo(const time_point_ms& to)
{
	if (!chunk)
		chunk_ = chunks_.pop();
//	time_point_ms tp = chunk_->timePoint();
	while (to > chunk_->timePoint())// + std::chrono::milliseconds((long int)chunk_->getDuration()))//
	{
		chunk_ = chunks_.pop();
		logD << "\nto: " << Chunk::getAge(to) << "\t chunk: " << Chunk::getAge(chunk_->timePoint()) << "\n";
		logD << "diff: " << std::chrono::duration_cast<std::chrono::milliseconds>((to - chunk_->timePoint())).count() << "\n";
	}
	chunk_->seek(std::chrono::duration_cast<std::chrono::milliseconds>(to - chunk_->timePoint()).count() * format_.msRate());
	return chunk_->timePoint();
}
*/

/*
time_point_hrc Stream::seek(long ms)
{
	if (!chunk)
		chunk_ = chunks_.pop();

	if (ms <= 0)
		return chunk_->start();

//	time_point_ms tp = chunk_->timePoint();
	while (ms > chunk_->duration<chronos::msec>().count())
	{
		chunk_ = chunks_.pop();
		ms -= min(ms, (long)chunk_->durationLeft<chronos::msec>().count());
	}
	chunk_->seek(ms * format_.msRate());
	return chunk_->start();
}
*/


time_point_hrc Stream::getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer)
{
	if (!chunk_ && !chunks_.try_pop(chunk_, timeout))
		throw 0;

	time_point_hrc tp = chunk_->start();
	char* buffer = (char*)outputBuffer;
	unsigned long read = 0;
	while (read < framesPerBuffer)
	{
		read += chunk_->readFrames(buffer + read*format_.frameSize, framesPerBuffer - read);
		if (chunk_->isEndOfChunk() && !chunks_.try_pop(chunk_, timeout))
			throw 0;
	}
	return tp;
}


time_point_hrc Stream::getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long framesPerBuffer, long framesCorrection)
{
	if (framesCorrection == 0)
		return getNextPlayerChunk(outputBuffer, timeout, framesPerBuffer);

	long toRead = framesPerBuffer + framesCorrection;
	char* buffer = (char*)malloc(toRead * format_.frameSize);
	time_point_hrc tp = getNextPlayerChunk(buffer, timeout, toRead);

	float factor = (float)toRead / framesPerBuffer;//(float)(framesPerBuffer*channels_);
	if (abs(framesCorrection) > 1)
		logO << "correction: " << framesCorrection << ", factor: " << factor << "\n";
	float idx = 0;
	for (size_t n=0; n<framesPerBuffer; ++n)
	{
		size_t index(floor(idx));// = (int)(ceil(n*factor));
		memcpy((char*)outputBuffer + n*format_.frameSize, buffer + index*format_.frameSize, format_.frameSize);
		idx += factor;
	}
	free(buffer);

	return tp;
}



void Stream::updateBuffers(int age)
{
	buffer_.add(age);
	miniBuffer_.add(age);
	shortBuffer_.add(age);
}


void Stream::resetBuffers()
{
	buffer_.clear();
	miniBuffer_.clear();
	shortBuffer_.clear();
}



bool Stream::getPlayerChunk(void* outputBuffer, const chronos::usec& outputBufferDacTime, unsigned long framesPerBuffer)
{
	if (outputBufferDacTime > bufferMs_)
		return false;

	if (!chunk_ && !chunks_.try_pop(chunk_, outputBufferDacTime))
		return false;

	playedFrames_ += framesPerBuffer;

	chronos::usec age = std::chrono::duration_cast<usec>(TimeProvider::serverNow() - chunk_->start() - bufferMs_ + outputBufferDacTime);
	if ((sleep_.count() == 0) && (chronos::abs(age) > chronos::msec(200)))
	{
		logO << "age > 200: " << age.count() << "\n";
		sleep_ = age;
	}

	try
	{

	//logD << "framesPerBuffer: " << framesPerBuffer << "\tms: " << framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE << "\t" << PLAYER_CHUNK_SIZE << "\n";
		chronos::nsec bufferDuration = chronos::nsec(chronos::usec::rep(framesPerBuffer / format_.nsRate()));
//		logD << "buffer duration: " << bufferDuration.count() << "\n";

		chronos::usec correction = chronos::usec(0);
		if (sleep_.count() != 0)
		{
			resetBuffers();
			if (sleep_ < -bufferDuration/2)
			{
				// We're early: not enough chunks_. play silence. Reference chunk_ is the oldest (front) one
				sleep_ = chrono::duration_cast<usec>(TimeProvider::serverNow() - getSilentPlayerChunk(outputBuffer, framesPerBuffer) - bufferMs_ + outputBufferDacTime);
//logD << "-sleep: " << sleep_.count() << " " << -bufferDuration.count() / 2000 << "\n";
				if (sleep_ < -bufferDuration/2)
					return true;
			}
			else if (sleep_ > bufferDuration/2)
			{
				// We're late: discard oldest chunks
				while (sleep_ > chunk_->duration<chronos::usec>())
				{
					logO << "sleep > chunk_->getDuration(): " << sleep_.count() << " > " << chunk_->duration<chronos::msec>().count() << ", chunks: " << chunks_.size() << ", out: " << outputBufferDacTime.count() << ", needed: " << bufferDuration.count() << "\n";
					sleep_ = std::chrono::duration_cast<usec>(TimeProvider::serverNow() - chunk_->start() - bufferMs_ + outputBufferDacTime);
					if (!chunks_.try_pop(chunk_, outputBufferDacTime))
					{
						logO << "no chunks available\n";
						chunk_ = NULL;
						sleep_ = chronos::usec(0);
						return false;
					}
				}
			}

			// out of sync, can be corrected by playing faster/slower
			if (sleep_ < -chronos::usec(100))
			{
				sleep_ += chronos::usec(100);
				correction = -chronos::usec(100);
			}
			else if (sleep_ > chronos::usec(100))
			{
				sleep_ -= chronos::usec(100);
				correction = chronos::usec(100);
			}
			else
			{
				logO << "Sleep " << sleep_.count() << "\n";
				correction = sleep_;
				sleep_ = chronos::usec(0);
			}
		}

		long framesCorrection = correction.count()*format_.usRate();
		if ((correctAfterXFrames_ != 0) && (playedFrames_ >= (unsigned long)abs(correctAfterXFrames_)))
		{
			framesCorrection += (correctAfterXFrames_ > 0)?1:-1;
			playedFrames_ -= abs(correctAfterXFrames_);
		}

		age = std::chrono::duration_cast<usec>(TimeProvider::serverNow() - getNextPlayerChunk(outputBuffer, outputBufferDacTime, framesPerBuffer, framesCorrection) - bufferMs_ + outputBufferDacTime);

		setRealSampleRate(format_.rate);
		if (sleep_.count() == 0)
		{
			if (buffer_.full())
			{
				if (chronos::usec(abs(median_)) > chronos::msec(1))
				{
					logO << "pBuffer->full() && (abs(median_) > 1): " << median_ << "\n";
					sleep_ = chronos::usec(shortMedian_);
				}
/*				else if (chronos::usec(median_) > chronos::usec(300))
				{
					setRealSampleRate(format_.rate - format_.rate / 1000);
				}
				else if (chronos::usec(median_) < -chronos::usec(300))
				{
					setRealSampleRate(format_.rate + format_.rate / 1000);
				}
*/			}
			else if (shortBuffer_.full())
			{
				if (chronos::usec(abs(shortMedian_)) > chronos::msec(5))
				{
					logO << "pShortBuffer->full() && (abs(shortMedian_) > 5): " << shortMedian_ << "\n";
					sleep_ = chronos::usec(shortMedian_);
				}
/*				else
				{
					setRealSampleRate(format_.rate + -shortMedian_ / 100);
				}
*/			}
			else if (miniBuffer_.full() && (chronos::usec(abs(miniBuffer_.median())) > chronos::msec(50)))
			{
				logO << "pMiniBuffer->full() && (abs(pMiniBuffer->mean()) > 50): " << miniBuffer_.median() << "\n";
				sleep_ = chronos::usec((chronos::msec::rep)miniBuffer_.mean());
			}
		}

		if (sleep_.count() != 0)
			logO << "Sleep: " << sleep_.count() << "\n";
		else if (shortBuffer_.full())
		{
			if (chronos::usec(shortMedian_) > chronos::usec(100))
				setRealSampleRate(format_.rate * 0.9999);
			else if (chronos::usec(shortMedian_) < -chronos::usec(100))
				setRealSampleRate(format_.rate * 1.0001);
		}

		updateBuffers(age.count());

		// print sync stats
		time_t now = time(NULL);
		if (now != lastUpdate_)
		{
			lastUpdate_ = now;
			median_ = buffer_.median();
			shortMedian_ = shortBuffer_.median();
			logO << "Chunk: " << age.count()/100 << "\t" << miniBuffer_.median()/100 << "\t" << shortMedian_/100 << "\t" << median_/100 << "\t" << buffer_.size() << "\t" << outputBufferDacTime.count() << "\n";
		}
		return true;
	}
	catch(int e)
	{
		sleep_ = chronos::usec(0);
		return false;
	}
}



