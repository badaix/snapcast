/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#include "stream.hpp"
#include "common/aixlog.hpp"
#include "time_provider.hpp"
#include <cmath>
#include <iostream>
#include <string.h>

using namespace std;
// using namespace chronos;
namespace cs = chronos;


Stream::Stream(const SampleFormat& sampleFormat)
    : format_(sampleFormat), sleep_(0), median_(0), shortMedian_(0), lastUpdate_(0), playedFrames_(0), bufferMs_(cs::msec(500))
{
    buffer_.setSize(500);
    shortBuffer_.setSize(100);
    miniBuffer_.setSize(20);
    //	cardBuffer_.setSize(50);

    // input_rate_ = format_.rate;
    // format_.rate = 48000;
    // output_rate_ = static_cast<double>(format_.rate);
    /*
    48000     x
    ------- = -----
    47999,2   x - 1

    x = 1,000016667 / (1,000016667 - 1)
    */
    setRealSampleRate(format_.rate);

    // soxr_error_t error;
    // soxr_io_spec_t iospec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
    // soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);
    // soxr_ = soxr_create(static_cast<double>(input_rate_), static_cast<double>(format_.rate), format_.channels, &error, &iospec, &q_spec, NULL);
    // if (error)
    // {
    //     LOG(ERROR) << "Error soxr_create: " << error << "\n";
    // }
}


void Stream::setRealSampleRate(double sampleRate)
{
    if (sampleRate == format_.rate)
        correctAfterXFrames_ = 0;
    else
        correctAfterXFrames_ = round((format_.rate / sampleRate) / (format_.rate / sampleRate - 1.));
    // LOG(DEBUG) << "Correct after X: " << correctAfterXFrames_ << " (Real rate: " << sampleRate << ", rate: " << format_.rate << ")\n";
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


void Stream::addChunk(unique_ptr<msg::PcmChunk> chunk)
{
    while (chunks_.size() * chunk->duration<cs::msec>().count() > 10000)
        chunks_.pop();

    chunks_.push(move(chunk));
    //	LOG(DEBUG) << "new chunk: " << chunk->duration<cs::msec>().count() << ", Chunks: " << chunks_.size() << "\n";

    // if (std::abs(input_rate_ - output_rate_) <= 0.0000001)
    // {
    //     chunks_.push(shared_ptr<msg::PcmChunk>(chunk));
    // }
    // else
    // {
    //     size_t idone;
    //     size_t odone;
    //     auto out = new msg::PcmChunk(format_, 0);
    //     out->timestamp = chunk->timestamp;
    //     out->payloadSize = ceil(chunk->payloadSize * static_cast<double>(output_rate_) / static_cast<double>(input_rate_));
    //     out->payload = (char*)malloc(out->payloadSize);

    //     soxr_io_spec_t iospec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
    //     soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);
    //     // auto error = soxr_oneshot(static_cast<double>(input_rate_), output_rate_, format_.channels, chunk->payload, chunk->getFrameCount(), &idone,
    //     // out->payload, out->payloadSize, &odone, &iospec, &q_spec, nullptr);
    //     auto error = soxr_process(soxr_, chunk->payload, chunk->getFrameCount(), &idone, out->payload, out->getFrameCount(), &odone);
    //     if (error)
    //     {
    //         LOG(ERROR) << "Error soxr_process: " << error << "\n";
    //         delete out;
    //     }
    //     else
    //     {
    //         out->payloadSize = odone * out->format.frameSize;
    //         LOG(TRACE) << "Resample idone: " << idone << "/" << chunk->getFrameCount() << ", odone: " << odone << "/"
    //                    << out->payloadSize / out->format.frameSize << "\n";
    //         chunks_.push(shared_ptr<msg::PcmChunk>(out));
    //     }
    //     delete chunk;
    // }
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
                LOG(DEBUG) << "\nto: " << Chunk::getAge(to) << "\t chunk: " << Chunk::getAge(chunk_->timePoint()) << "\n";
                LOG(DEBUG) << "diff: " << std::chrono::duration_cast<std::chrono::milliseconds>((to - chunk_->timePoint())).count() << "\n";
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
    unsigned long read = 0;
    while (read < framesPerBuffer)
    {
        read += chunk_->readFrames(static_cast<char*>(outputBuffer) + read * format_.frameSize, framesPerBuffer - read);
        if (chunk_->isEndOfChunk() && !chunks_.try_pop(chunk_, timeout))
            throw 0;
    }
    return tp;
}


cs::time_point_clk Stream::getNextPlayerChunk(void* outputBuffer, const cs::usec& timeout, unsigned long framesPerBuffer, long framesCorrection)
{
    if (framesCorrection < 0 && framesPerBuffer + framesCorrection <= 0)
    {
        // Avoid underflow in new char[] constructor.
        framesCorrection = -framesPerBuffer + 1;
    }

    if (framesCorrection == 0)
        return getNextPlayerChunk(outputBuffer, timeout, framesPerBuffer);

    long toRead = framesPerBuffer + framesCorrection;
    char* buffer = new char[toRead * format_.frameSize];
    cs::time_point_clk tp = getNextPlayerChunk(buffer, timeout, toRead);

    const auto max = framesCorrection < 0 ? framesPerBuffer : toRead;
    // Divide the buffer into one more slice than frames that need to be dropped.
    // We will drop/repeat 0 frames from the first slice, 1 frame from the second, ..., and framesCorrection frames from the last slice.
    size_t slices = abs(framesCorrection) + 1;
    if (slices > max)
    {
        // We cannot have more slices than frames, because that would cause
        // size = 0 -> pos = 0 -> pos - n < 0 in the loop below
        // Overwriting slices effectively corrects less frames than asked for in framesCorrection.
        slices = max;
    }
    // Size of each slice. The last slice may be bigger.
    int size = max / slices;

    // LOG(TRACE) << "getNextPlayerChunk, frames: " << framesPerBuffer << ", correction: " << framesCorrection << " (" << toRead << "), slices: " << slices
    // << "\n";

    size_t pos = 0;
    for (size_t n = 0; n < slices; ++n)
    {
        if (n + 1 == slices)
            // Adjust size in the last iteration, because the last slice may be bigger
            size = max - pos;

        if (framesCorrection < 0)
        {
            // Read one frame less per slice from the input, but write a duplicated frame per slice to the output

            // LOG(TRACE) << "slice: " << n << ", size: " << size << ", out pos: " << pos << ", source pos: " << pos - n << "\n";
            memcpy(static_cast<char*>(outputBuffer) + pos * format_.frameSize, buffer + (pos - n) * format_.frameSize, size * format_.frameSize);
        }
        else
        {
            // Read all input frames, but skip a frame per slice when writing to the output.

            // LOG(TRACE) << "slice: " << n << ", size: " << size << ", out pos: " << pos - n << ", source pos: " << pos << "\n";
            memcpy(static_cast<char*>(outputBuffer) + (pos - n) * format_.frameSize, buffer + pos * format_.frameSize, size * format_.frameSize);
        }
        pos += size;
    }

    // float idx = 0;
    // for (size_t n = 0; n < framesPerBuffer; ++n)
    // {
    //     size_t index(floor(idx)); // = (int)(ceil(n*factor));
    //     memcpy((char*)outputBuffer + n * format_.frameSize, buffer + index * format_.frameSize, format_.frameSize);
    //     idx += factor;
    // }
    delete[] buffer;

    return tp;
}

/*
2020-01-12 20-25-26 [Info] Chunk: 7	7	11	15	179	120
2020-01-12 20-25-27 [Info] Chunk: 6	6	8	15	212	122
2020-01-12 20-25-28 [Info] Chunk: 6	6	7	12	245	123
2020-01-12 20-25-29 [Info] Chunk: 5	6	6	9	279	117
2020-01-12 20-25-30 [Info] Chunk: 4	5	6	8	312	117
2020-01-12 20-25-30 [Error] Controller::onException: read_some: End of file
2020-01-12 20-25-30 [Error] Exception in Controller::worker(): read_some: End of file
2020-01-12 20-25-31 [Error] Exception in Controller::worker(): connect: Connection refused
2020-01-12 20-25-31 [Error] Error in socket shutdown: Transport endpoint is not connected
2020-01-12 20-25-32 [Error] Exception in Controller::worker(): connect: Connection refused
2020-01-12 20-25-32 [Error] Error in socket shutdown: Transport endpoint is not connected
^C2020-01-12 20-25-32 [Info] Received signal 2: Interrupt
2020-01-12 20-25-32 [Info] Stopping controller
2020-01-12 20-25-32 [Error] Error in socket shutdown: Bad file descriptor
2020-01-12 20-25-32 [Error] Exception: Invalid argument
2020-01-12 20-25-32 [Notice] daemon terminated.

=================================================================
==22383==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 5756 byte(s) in 1 object(s) allocated from:
    #0 0x7f3d60635602 in malloc (/usr/lib/x86_64-linux-gnu/libasan.so.2+0x98602)
    #1 0x448fc2 in Stream::getNextPlayerChunk(void*, std::chrono::duration<long, std::ratio<1l, 1000000l> > const&, unsigned long, long)
/home/johannes/Develop/snapcast/client/stream.cpp:163

SUMMARY: AddressSanitizer: 5756 byte(s) leaked in 1 allocation(s).
*/

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
        LOG(INFO) << "outputBufferDacTime > bufferMs: " << cs::duration<cs::msec>(outputBufferDacTime) << " > " << cs::duration<cs::msec>(bufferMs_) << "\n";
        sleep_ = cs::usec(0);
        return false;
    }

    if (!chunk_ && !chunks_.try_pop(chunk_, outputBufferDacTime))
    {
        // LOG(INFO) << "no chunks available\n";
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
    //	LOG(INFO) << "age: " << age.count() / 1000 << "\n";
    if ((sleep_.count() == 0) && (cs::abs(age) > cs::msec(200)))
    {
        LOG(INFO) << "age > 200: " << cs::duration<cs::msec>(age) << "\n";
        sleep_ = age;
    }

    try
    {

        // LOG(DEBUG) << "framesPerBuffer: " << framesPerBuffer << "\tms: " << framesPerBuffer*2 / PLAYER_CHUNK_MS_SIZE << "\t" << PLAYER_CHUNK_SIZE << "\n";
        cs::nsec bufferDuration = cs::nsec(static_cast<cs::nsec::rep>(framesPerBuffer / format_.nsRate()));
        //		LOG(DEBUG) << "buffer duration: " << bufferDuration.count() << "\n";

        cs::usec correction = cs::usec(0);
        if (sleep_.count() != 0)
        {
            resetBuffers();
            if (sleep_ < -bufferDuration / 2)
            {
                LOG(INFO) << "sleep < -bufferDuration/2: " << cs::duration<cs::msec>(sleep_) << " < " << -cs::duration<cs::msec>(bufferDuration) / 2 << ", ";
                // We're early: not enough chunks_. play silence. Reference chunk_ is the oldest (front) one
                sleep_ = chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - getSilentPlayerChunk(outputBuffer, framesPerBuffer) - bufferMs_ +
                                                         outputBufferDacTime);
                LOG(INFO) << "sleep: " << cs::duration<cs::msec>(sleep_) << "\n";
                if (sleep_ < -bufferDuration / 2)
                    return true;
            }
            else if (sleep_ > bufferDuration / 2)
            {
                LOG(INFO) << "sleep > bufferDuration/2: " << cs::duration<cs::msec>(sleep_) << " > " << cs::duration<cs::msec>(bufferDuration) / 2 << "\n";
                // We're late: discard oldest chunks
                while (sleep_ > chunk_->duration<cs::usec>())
                {
                    LOG(INFO) << "sleep > chunkDuration: " << cs::duration<cs::msec>(sleep_) << " > " << chunk_->duration<cs::msec>().count()
                              << ", chunks: " << chunks_.size() << ", out: " << cs::duration<cs::msec>(outputBufferDacTime)
                              << ", needed: " << cs::duration<cs::msec>(bufferDuration) << "\n";
                    sleep_ = std::chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - chunk_->start() - bufferMs_ + outputBufferDacTime);
                    if (!chunks_.try_pop(chunk_, outputBufferDacTime))
                    {
                        LOG(INFO) << "no chunks available\n";
                        chunk_ = nullptr;
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
                LOG(INFO) << "Sleep " << cs::duration<cs::msec>(sleep_) << "\n";
                correction = sleep_;
                sleep_ = cs::usec(0);
            }
        }

        // framesCorrection = number of frames to be read more or less to get in-sync
        long framesCorrection = correction.count() * format_.usRate();

        // sample rate correction
        if ((correctAfterXFrames_ != 0) && (playedFrames_ >= (unsigned long)abs(correctAfterXFrames_)))
        {
            framesCorrection += (correctAfterXFrames_ > 0) ? 1 : -1;
            playedFrames_ = 0; //-= abs(correctAfterXFrames_);
        }

        age = std::chrono::duration_cast<cs::usec>(TimeProvider::serverNow() -
                                                   getNextPlayerChunk(outputBuffer, outputBufferDacTime, framesPerBuffer, framesCorrection) - bufferMs_ +
                                                   outputBufferDacTime);

        setRealSampleRate(format_.rate);
        if (sleep_.count() == 0)
        {
            if (buffer_.full())
            {
                if (cs::usec(abs(median_)) > cs::msec(2))
                {
                    LOG(INFO) << "pBuffer->full() && (abs(median_) > 2): " << median_ << "\n";
                    sleep_ = cs::usec(median_);
                }
                // else if (cs::usec(median_) > cs::usec(300))
                // {
                //     setRealSampleRate(format_.rate - format_.rate / 1000);
                // }
                // else if (cs::usec(median_) < -cs::usec(300))
                // {
                //     setRealSampleRate(format_.rate + format_.rate / 1000);
                // }
            }
            else if (shortBuffer_.full())
            {
                if (cs::usec(abs(shortMedian_)) > cs::msec(5))
                {
                    LOG(INFO) << "pShortBuffer->full() && (abs(shortMedian_) > 5): " << shortMedian_ << "\n";
                    sleep_ = cs::usec(shortMedian_);
                }
                // else
                // {
                //     setRealSampleRate(format_.rate + -shortMedian_ / 100);
                // }
            }
            else if (miniBuffer_.full() && (cs::usec(abs(miniBuffer_.median())) > cs::msec(50)))
            {
                LOG(INFO) << "pMiniBuffer->full() && (abs(pMiniBuffer->mean()) > 50): " << miniBuffer_.median() << "\n";
                sleep_ = cs::usec(static_cast<cs::msec::rep>(miniBuffer_.mean()));
            }
        }

        if (sleep_.count() != 0)
        {
            static int lastAge(0);
            int msAge = cs::duration<cs::msec>(age);
            if (lastAge != msAge)
            {
                lastAge = msAge;
                LOG(INFO) << "Sleep " << cs::duration<cs::msec>(sleep_) << ", age: " << msAge << ", bufferDuration: " << cs::duration<cs::msec>(bufferDuration)
                          << "\n";
            }
        }
        else if (shortBuffer_.full())
        {
            auto miniMedian = miniBuffer_.median();
            if ((cs::usec(shortMedian_) > cs::usec(100)) && (cs::usec(miniMedian) > cs::usec(50)) && (cs::usec(age) > cs::usec(50)))
            {
                double rate = (shortMedian_ / 100) * 0.00005;
                rate = 1.0 - std::min(rate, 0.0005);
                // LOG(INFO) << "Rate: " << rate << "\n";
                setRealSampleRate(format_.rate * rate); // 0.9999);
            }
            else if ((cs::usec(shortMedian_) < -cs::usec(100)) && (cs::usec(miniMedian) < -cs::usec(50)) && (cs::usec(age) < -cs::usec(50)))
            {
                double rate = (-shortMedian_ / 100) * 0.00005;
                rate = 1.0 + std::min(rate, 0.0005);
                // LOG(INFO) << "Rate: " << rate << "\n";
                setRealSampleRate(format_.rate * rate); // 1.0001);
            }
        }

        updateBuffers(age.count());

        // print sync stats
        time_t now = time(nullptr);
        if (now != lastUpdate_)
        {
            lastUpdate_ = now;
            median_ = buffer_.median();
            shortMedian_ = shortBuffer_.median();
            LOG(INFO) << "Chunk: " << age.count() / 100 << "\t" << miniBuffer_.median() / 100 << "\t" << shortMedian_ / 100 << "\t" << median_ / 100 << "\t"
                      << buffer_.size() << "\t" << cs::duration<cs::msec>(outputBufferDacTime) << "\n";
            // LOG(INFO) << "Chunk: " << age.count()/1000 << "\t" << miniBuffer_.median()/1000 << "\t" << shortMedian_/1000 << "\t" << median_/1000 << "\t" <<
            // buffer_.size() << "\t" << cs::duration<cs::msec>(outputBufferDacTime) << "\n";
        }
        return (abs(cs::duration<cs::msec>(age)) < 500);
    }
    catch (int e)
    {
        sleep_ = cs::usec(0);
        return false;
    }
}
