/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include "stream.h"
#include <cmath>
#include <iostream>
#include <string.h>
#include "common/log.h"
#include "timeProvider.h"

using namespace std;
//using namespace chronos;
namespace cs = chronos;


Stream::Stream(const SampleFormat& sampleFormat) : format_(sampleFormat), sleep_(0), median_(0), shortMedian_(0), lastUpdate_(0), playedFrames_(0), bufferMs_(cs::msec(500))
{
	buffer_.setSize(500);
	shortBuffer_.setSize(100);
	miniBuffer_.setSize(20);
//	cardBuffer_.setSize(50);

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
	bufferMs_ = cs::msec(bufferLenMs);
}



void Stream::clearChunks()
{
	while (chunks_.size() > 0)
		chunks_.pop();
	resetBuffers();
}


void Stream::addChunk(msg::PcmChunk* chunk)
{
	while (chunks_.size() * chunk->duration<cs::msec>().count() > 10000)
		chunks_.pop();
	chunks_.push(shared_ptr<msg::PcmChunk>(chunk));
//	logD << "new chunk: " << chunk->duration<cs::msec>().count() << ", Chunks: " << chunks_.size() << "\n";
}


bool Stream::waitForChunk(size_t ms) const
{
	return chunks_.wait_for(std::chrono::milliseconds(ms));
}



cs::time_point_clk Stream::getSilentPlayerChunk(void* outputBuffer, unsigned long framesPerBuffer)
{
	if (!chunk_)
		chunk_ = chunks_.pop();
	cs::time_point_clk tp = chunk_->start();
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
time_point_clk Stream::seek(long ms)
{
	if (!chunk)
		chunk_ = chunks_.pop();

	if (ms <= 0)
		return chunk_->start();

//	time_point_ms tp = chunk_->timePoint();
	while (ms > chunk_->duration<cs::msec>().count())
	{
		chunk_ = chunks_.pop();
		ms -= min(ms, (long)chunk_->durationLeft<cs::msec>().count());
	}
	chunk_->seek(ms * format_.msRate());
	return chunk_->start();
}
*/


cs::time_point_clk Stream::getNextPlayerChunk(void* outputBuffer, const cs::usec& timeout, unsigned long framesPerBuffer)
{
	if (!chunk_ && !chunks_.try_pop(chunk_, timeout))
		throw 0;

	cs::time_point_clk tp = chunk_->start();
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


cs::time_point_clk Stream::getNextPlayerChunk(void* outputBuffer, const cs::usec& timeout, unsigned long framesPerBuffer, long framesCorrection)
{
	if (framesCorrection == 0)
		return getNextPlayerChunk(outputBuffer, timeout, framesPerBuffer);

	long toRead = framesPerBuffer + framesCorrection;
	char* buffer = (char*)malloc(toRead * format_.frameSize);
	cs::time_point_clk tp = getNextPlayerChunk(buffer, timeout, toRead);

	float factor = (float)toRead / framesPerBuffer;//(float)(framesPerBuffer*channels_);
//	if (abs(framesCorrection) > 1)
//		logO << "correction: " << framesCorrection << ", factor: " << factor << "\n";
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



bool Stream::getPlayerChunk(void* outputBuffer, const cs::usec& outputBufferDacTime, unsigned long framesPerBuffer)
{
	if (outputBufferDacTime > bufferMs_)
	{
		logO << "outputBufferDacTime > bufferMs: " << cs::duration<cs::msec>(outputBufferDacTime) << " > " << cs::duration<cs::msec>(bufferMs_) << "\n";
		sleep_ = cs::usec(0);
		return false;
	}

	if (!chunk_ && !chunks_.try_pop(chunk_, outputBufferDacTime))
	{
		logO << "no chunks available\n";
		sleep_ = cs::usec(0);
		return false;
	}

	playedFrames_ += framesPerBuffer;

	/// we have a chunk
	/// age = chunk age (server now - rec time: some positive value) - buffer (e.g. 1000ms) + time to DAC
	/// age = 0 => play now
	/// age < 0 => play in -age
	/// age > 0 => too old
	cs::usec age = std::chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - chunk_->start()) - bufferMs_ + outputBufferDacTime;
//	logO << "age: " << age.count() / 1000 << "\n";
	if ((sleep_.count() == 0) && (cs::abs(age) > cs::msec(200)))
	{
		logO << "age > 200: " << cs::duration<cs::msec>(age) << "\n";
		sleep_ = age;
	}

	try
	{

	//logD << "framesPerBuffer: " << framesPerBuffer << "\tms: " << framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE << "\t" << PLAYER_CHUNK_SIZE << "\n";
		cs::nsec bufferDuration = cs::nsec(cs::nsec::rep(framesPerBuffer / format_.nsRate()));
//		logD << "buffer duration: " << bufferDuration.count() << "\n";

		cs::usec correction = cs::usec(0);
		if (sleep_.count() != 0)
		{
			resetBuffers();
			if (sleep_ < -bufferDuration/2)
			{
				logO << "sleep < -bufferDuration/2: " << cs::duration<cs::msec>(sleep_) << " < " << -cs::duration<cs::msec>(bufferDuration)/2 << ", ";
				// We're early: not enough chunks_. play silence. Reference chunk_ is the oldest (front) one
				sleep_ = chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - getSilentPlayerChunk(outputBuffer, framesPerBuffer) - bufferMs_ + outputBufferDacTime);
				logO << "sleep: " << cs::duration<cs::msec>(sleep_) << "\n";
				if (sleep_ < -bufferDuration/2)
					return true;
			}
			else if (sleep_ > bufferDuration/2)
			{
				logO << "sleep > bufferDuration/2: " << cs::duration<cs::msec>(sleep_) << " > " << cs::duration<cs::msec>(bufferDuration)/2 << "\n";
				// We're late: discard oldest chunks
				while (sleep_ > chunk_->duration<cs::usec>())
				{
					logO << "sleep > chunkDuration: " << cs::duration<cs::msec>(sleep_) << " > " << chunk_->duration<cs::msec>().count() << ", chunks: " << chunks_.size() << ", out: " << cs::duration<cs::msec>(outputBufferDacTime) << ", needed: " << cs::duration<cs::msec>(bufferDuration) << "\n";
					sleep_ = std::chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - chunk_->start() - bufferMs_ + outputBufferDacTime);
					if (!chunks_.try_pop(chunk_, outputBufferDacTime))
					{
						logO << "no chunks available\n";
						chunk_ = NULL;
						sleep_ = cs::usec(0);
						return false;
					}
				}
			}

			// out of sync, can be corrected by playing faster/slower
			if (sleep_ < -cs::usec(100))
			{
				sleep_ += cs::usec(100);
				correction = -cs::usec(100);
			}
			else if (sleep_ > cs::usec(100))
			{
				sleep_ -= cs::usec(100);
				correction = cs::usec(100);
			}
			else
			{
				logO << "Sleep " << cs::duration<cs::msec>(sleep_) << "\n";
				correction = sleep_;
				sleep_ = cs::usec(0);
			}
		}

		// framesCorrection = number of frames to be read more or less to get in-sync
		long framesCorrection = correction.count()*format_.usRate();

		// sample rate correction
		if ((correctAfterXFrames_ != 0) && (playedFrames_ >= (unsigned long)abs(correctAfterXFrames_)))
		{
			framesCorrection += (correctAfterXFrames_ > 0)?1:-1;
			playedFrames_ -= abs(correctAfterXFrames_);
		}

		age = std::chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - getNextPlayerChunk(outputBuffer, outputBufferDacTime, framesPerBuffer, framesCorrection) - bufferMs_ + outputBufferDacTime);

		setRealSampleRate(format_.rate);
		if (sleep_.count() == 0)
		{
			if (buffer_.full())
			{
				if (cs::usec(abs(median_)) > cs::msec(1))
				{
					logO << "pBuffer->full() && (abs(median_) > 1): " << median_ << "\n";
					sleep_ = cs::usec(median_);
				}
/*				else if (cs::usec(median_) > cs::usec(300))
				{
					setRealSampleRate(format_.rate - format_.rate / 1000);
				}
				else if (cs::usec(median_) < -cs::usec(300))
				{
					setRealSampleRate(format_.rate + format_.rate / 1000);
				}
*/			}
			else if (shortBuffer_.full())
			{
				if (cs::usec(abs(shortMedian_)) > cs::msec(5))
				{
					logO << "pShortBuffer->full() && (abs(shortMedian_) > 5): " << shortMedian_ << "\n";
					sleep_ = cs::usec(shortMedian_);
				}
/*				else
				{
					setRealSampleRate(format_.rate + -shortMedian_ / 100);
				}
*/			}
			else if (miniBuffer_.full() && (cs::usec(abs(miniBuffer_.median())) > cs::msec(50)))
			{
				logO << "pMiniBuffer->full() && (abs(pMiniBuffer->mean()) > 50): " << miniBuffer_.median() << "\n";
				sleep_ = cs::usec((cs::msec::rep)miniBuffer_.mean());
			}
		}

		if (sleep_.count() != 0)
		{
			static int lastAge(0);
			int msAge = cs::duration<cs::msec>(age);
			if (lastAge != msAge) 
			{
				lastAge = msAge;
				logO << "Sleep " << cs::duration<cs::msec>(sleep_) << ", age: " << msAge << ", bufferDuration: " << cs::duration<cs::msec>(bufferDuration) << "\n";
			}
		}
		else if (shortBuffer_.full())
		{
			if (cs::usec(shortMedian_) > cs::usec(100))
				setRealSampleRate(format_.rate * 0.9999);
			else if (cs::usec(shortMedian_) < -cs::usec(100))
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
			logO << "Chunk: " << age.count()/100 << "\t" << miniBuffer_.median()/100 << "\t" << shortMedian_/100 << "\t" << median_/100 << "\t" << buffer_.size() << "\t" << cs::duration<cs::msec>(outputBufferDacTime) << "\n";
//			logO << "Chunk: " << age.count()/1000 << "\t" << miniBuffer_.median()/1000 << "\t" << shortMedian_/1000 << "\t" << median_/1000 << "\t" << buffer_.size() << "\t" << cs::duration<cs::msec>(outputBufferDacTime) << "\n";
		}
		return (abs(cs::duration<cs::msec>(age)) < 500);
	}
	catch(int e)
	{
		sleep_ = cs::usec(0);
		return false;
	}
}



