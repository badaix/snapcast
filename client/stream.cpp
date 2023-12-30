/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include "stream.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/logging.hpp"
#include "time_provider.hpp"
#include <cmath>
#include <cstring>
#include <iostream>


using namespace std;
namespace cs = chronos;

static constexpr auto LOG_TAG = "Stream";
static constexpr auto kCorrectionBegin = 100us;

// #define LOG_LATENCIES

Stream::Stream(const SampleFormat& in_format, const SampleFormat& out_format)
    : in_format_(in_format), median_(0), shortMedian_(0), lastUpdate_(0), playedFrames_(0), correctAfterXFrames_(0), bufferMs_(cs::msec(500)), frame_delta_(0),
      hard_sync_(true)
{
    buffer_.setSize(500);
    shortBuffer_.setSize(100);
    miniBuffer_.setSize(20);
    latencies_.setSize(100);

    format_ = in_format_;
    if (out_format.isInitialized())
    {
        format_.setFormat(out_format.rate() != 0 ? out_format.rate() : format_.rate(), out_format.bits() != 0 ? out_format.bits() : format_.bits(),
                          out_format.channels() != 0 ? out_format.channels() : format_.channels());
    }

    /*
    48000     x
    ------- = -----
    47999,2   x - 1

    x = 1,000016667 / (1,000016667 - 1)
    */
    // setRealSampleRate(format_.rate());
    resampler_ = std::make_unique<Resampler>(in_format_, format_);
}


void Stream::setRealSampleRate(double sampleRate)
{
    if (sampleRate == format_.rate())
    {
        correctAfterXFrames_ = 0;
    }
    else
    {
        correctAfterXFrames_ = static_cast<int32_t>(round((format_.rate() / sampleRate) / (format_.rate() / sampleRate - 1.)));
        // LOG(TRACE, LOG_TAG) << "Correct after X: " << correctAfterXFrames_ << " (Real rate: " << sampleRate << ", rate: " << format_.rate() << ")\n";
    }
}


void Stream::setBufferLen(size_t bufferLenMs)
{
    bufferMs_.store(cs::msec(bufferLenMs));
}


void Stream::clearChunks()
{
    std::lock_guard<std::mutex> lock(mutex_);
    while (!chunks_.empty())
        chunks_.pop();
    resetBuffers();
}


void Stream::addChunk(unique_ptr<msg::PcmChunk> chunk)
{
    // drop chunk if it's too old. Just in case, this shouldn't happen.
    auto age = std::chrono::duration_cast<cs::msec>(TimeProvider::serverNow() - chunk->start());
    if (age > 5s + bufferMs_.load())
        return;

    auto resampled = resampler_->resample(std::move(chunk));
    if (resampled)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recent_ = resampled;
        chunks_.push(resampled);

        std::shared_ptr<msg::PcmChunk> front_;
        while (chunks_.front_copy(front_))
        {
            age = std::chrono::duration_cast<cs::msec>(TimeProvider::serverNow() - front_->start());
            if ((age > 5s + bufferMs_.load()) && chunks_.try_pop(front_))
                LOG(TRACE, LOG_TAG) << "Oldest chunk too old: " << age.count() << " ms, removing. Chunks in queue left: " << chunks_.size() << "\n";
            else
                break;
        }
    }
    // LOG(TRACE, LOG_TAG) << "new chunk: " << chunk->durationMs() << " ms, age: " << age.count() << " ms, Chunks: " << chunks_.size() << "\n";
}


bool Stream::waitForChunk(const std::chrono::milliseconds& timeout) const
{
    return chunks_.wait_for(timeout);
}


void Stream::getSilentPlayerChunk(void* outputBuffer, uint32_t frames) const
{
    memset(outputBuffer, 0, frames * format_.frameSize());
}


cs::time_point_clk Stream::getNextPlayerChunk(void* outputBuffer, uint32_t frames)
{
    if (!chunk_ && !chunks_.try_pop(chunk_))
        throw SnapException("No chunks available, requested frames: " + cpt::to_string(frames));

    cs::time_point_clk tp = chunk_->start();
    uint32_t read = 0;
    while (read < frames)
    {
        read += chunk_->readFrames(static_cast<char*>(outputBuffer) + read * format_.frameSize(), frames - read);
        if ((read < frames) && chunk_->isEndOfChunk() && !chunks_.try_pop(chunk_))
            throw SnapException("Not enough frames available, requested frames: " + cpt::to_string(frames) + ", available: " + cpt::to_string(read));
    }
    return tp;
}


cs::time_point_clk Stream::getNextPlayerChunk(void* outputBuffer, uint32_t frames, int32_t framesCorrection)
{
    if (framesCorrection < 0 && (static_cast<int32_t>(frames) + framesCorrection <= 0))
    {
        // Avoid underflow in new char[] constructor.
        framesCorrection = -static_cast<int32_t>(frames) + 1;
    }

    if (framesCorrection == 0)
        return getNextPlayerChunk(outputBuffer, frames);

    frame_delta_ -= framesCorrection;

    uint32_t toRead = frames + framesCorrection;
    if (toRead * format_.frameSize() > read_buffer_.size())
        read_buffer_.resize(toRead * format_.frameSize());
    cs::time_point_clk tp = getNextPlayerChunk(read_buffer_.data(), toRead);

    const auto max = framesCorrection < 0 ? frames : toRead;
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
    auto size = max / slices;

    // LOG(TRACE, LOG_TAG) << "getNextPlayerChunk, frames: " << frames << ", correction: " << framesCorrection << " (" << toRead << "), slices: " << slices
    // << "\n";

    size_t pos = 0;
    for (size_t n = 0; n < slices; ++n)
    {
        // Adjust size in the last iteration, because the last slice may be bigger
        if (n + 1 == slices)
            size = max - pos;

        if (framesCorrection < 0)
        {
            // Read one frame less per slice from the input, but write a duplicated frame per slice to the output
            // LOG(TRACE, LOG_TAG) << "duplicate - requested: " << frames << ", read: " << toRead << ", slice: " << n << ", size: " << size << ", out pos: " <<
            // pos << ", source pos: " << pos - n << "\n";
            memcpy(static_cast<char*>(outputBuffer) + pos * format_.frameSize(), read_buffer_.data() + (pos - n) * format_.frameSize(),
                   size * format_.frameSize());
        }
        //    --      --
        //   -  -    -  -
        //  -    -  -    -
        // -      --      -

        //    --       --
        //   -  -     -  -
        //  -    --  -    -
        // -       --      -

        //    --       --
        //   -  -     -  -
        //  -    -_  -    -
        // -       --      -

        else
        {
            // Read all input frames, but skip a frame per slice when writing to the output.
            // LOG(TRACE, LOG_TAG) << "remove - requested: " << frames << ", read: " << toRead << ", slice: " << n << ", size: " << size << ", out pos: " << pos
            // - n << ", source pos: " << pos << "\n";
            memcpy(static_cast<char*>(outputBuffer) + (pos - n) * format_.frameSize(), read_buffer_.data() + pos * format_.frameSize(),
                   size * format_.frameSize());
        }
        pos += size;
    }

    return tp;
}


void Stream::updateBuffers(chronos::usec::rep age)
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


bool Stream::getPlayerChunk(void* outputBuffer, const cs::usec& outputBufferDacTime, uint32_t frames)
{
    if (outputBufferDacTime > bufferMs_.load())
    {
        LOG(INFO, LOG_TAG) << "outputBufferDacTime > bufferMs: " << cs::duration<cs::msec>(outputBufferDacTime) << " > "
                           << cs::duration<cs::msec>(bufferMs_.load()) << "\n";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    time_t now = time(nullptr);
    if (!chunk_ && !chunks_.try_pop(chunk_))
    {
        if (now != lastUpdate_)
        {
            lastUpdate_ = now;
            LOG(INFO, LOG_TAG) << "No chunks available\n";
        }
        return false;
    }

#ifdef LOG_LATENCIES
    // calculate the estimated end to end latency
    if (recent_)
    {
        cs::nsec req_chunk_duration = cs::nsec(static_cast<cs::nsec::rep>(frames / format_.nsRate()));
        auto youngest = recent_->end() - req_chunk_duration;
        cs::msec age = std::chrono::duration_cast<cs::msec>(TimeProvider::serverNow() - youngest + outputBufferDacTime);
        latencies_.add(age.count());
    }
#endif

    /// we have a chunk
    /// age = chunk age (server now - rec time: some positive value) - buffer (e.g. 1000ms) + time to DAC
    /// age = 0 => play now
    /// age < 0 => play in -age => wait for a while, play silence in the meantime
    /// age > 0 => too old      => throw them away

    try
    {
        if (hard_sync_)
        {
            cs::nsec req_chunk_duration = cs::nsec(static_cast<cs::nsec::rep>(frames / format_.nsRate()));
            cs::usec age = std::chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - chunk_->start()) - bufferMs_.load() + outputBufferDacTime;
            // LOG(INFO, LOG_TAG) << "age: " << age.count() / 1000 << ", buffer: " <<
            // std::chrono::duration_cast<chrono::milliseconds>(req_chunk_duration).count() << "\n";
            if (age < -req_chunk_duration)
            {
                // the oldest chunk (top of the stream) is too young for the buffer
                // e.g. age = -100ms (=> should be played in 100ms)
                // but the requested chunk duration is 50ms, so there is not data in this iteration available
                getSilentPlayerChunk(outputBuffer, frames);
                return true;
            }
            else
            {
                if (age.count() > 0)
                {
                    LOG(DEBUG, LOG_TAG) << "age > 0: " << age.count() / 1000 << "ms, dropping old chunks\n";
                    // age > 0: the top of the stream is too old. We must fast foward.
                    // delete the current chunk, it's too old. This will avoid an endless loop if there is no chunk in the queue.
                    chunk_ = nullptr;
                    while (chunks_.try_pop(chunk_))
                    {
                        age = std::chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - chunk_->start()) - bufferMs_.load() + outputBufferDacTime;
                        LOG(DEBUG, LOG_TAG) << "age: " << age.count() / 1000 << ", requested chunk_duration: "
                                            << std::chrono::duration_cast<std::chrono::milliseconds>(req_chunk_duration).count()
                                            << ", duration: " << chunk_->duration<std::chrono::milliseconds>().count() << "\n";
                        // check if the current chunk's end is older than age => can be player
                        if ((age.count() > 0) && (age < chunk_->duration<cs::usec>()))
                        {
                            // fast forward by "age" to get in sync, i.e. age = 0
                            chunk_->seek(static_cast<uint32_t>(chunk_->format.nsRate() * std::chrono::duration_cast<cs::nsec>(age).count()));
                            age = 0s;
                        }
                        if (age.count() <= 0)
                            break;
                    }
                }

                if (age.count() <= 0)
                {
                    // the oldest chunk (top of the stream) can be played in this iteration
                    // e.g. age = -20ms (=> should be played in 20ms)
                    // and the current chunk duration is 50ms, so we need to play 20ms silence (as we don't have data)
                    // and can play 30ms of the stream
                    uint32_t silent_frames = static_cast<uint32_t>(-chunk_->format.nsRate() * std::chrono::duration_cast<cs::nsec>(age).count());
                    bool result = (silent_frames <= frames);
                    silent_frames = std::min(silent_frames, frames);
                    if (silent_frames > 0)
                    {
                        LOG(DEBUG, LOG_TAG) << "Silent frames: " << silent_frames << ", frames: " << frames
                                            << ", age: " << std::chrono::duration_cast<cs::usec>(age).count() / 1000. << "\n";
                        getSilentPlayerChunk(outputBuffer, silent_frames);
                    }
                    getNextPlayerChunk(static_cast<char*>(outputBuffer) + (chunk_->format.frameSize() * silent_frames), frames - silent_frames);

                    if (result)
                    {
                        hard_sync_ = false;
                        resetBuffers();
                    }
                    return true;
                }
                return false;
            }
        }

        // sample rate correction
        // framesCorrection = number of frames to be read more or less to get in-sync
        int32_t framesCorrection = 0;
        if (correctAfterXFrames_ != 0)
        {
            playedFrames_ += frames;
            if (playedFrames_ >= static_cast<uint32_t>(abs(correctAfterXFrames_)))
            {
                framesCorrection = static_cast<int32_t>(playedFrames_) / correctAfterXFrames_;
                playedFrames_ %= abs(correctAfterXFrames_);
            }
        }

        cs::usec age = std::chrono::duration_cast<cs::usec>(TimeProvider::serverNow() - getNextPlayerChunk(outputBuffer, frames, framesCorrection) -
                                                            bufferMs_.load() + outputBufferDacTime);

        setRealSampleRate(format_.rate());
        // check if we need a hard sync
        if (buffer_.full() && (cs::usec(abs(median_)) > cs::msec(2)) && (cs::abs(age) > cs::usec(500)))
        {
            LOG(INFO, LOG_TAG) << "pBuffer->full() && (abs(median_) > 2): " << median_ << "\n";
            hard_sync_ = true;
        }
        else if (shortBuffer_.full() && (cs::usec(abs(shortMedian_)) > cs::msec(5)) && (cs::abs(age) > cs::usec(500)))
        {
            LOG(INFO, LOG_TAG) << "pShortBuffer->full() && (abs(shortMedian_) > 5): " << shortMedian_ << "\n";
            hard_sync_ = true;
        }
        else if (miniBuffer_.full() && (cs::usec(abs(miniBuffer_.median())) > cs::msec(50)) && (cs::abs(age) > cs::usec(500)))
        {
            LOG(INFO, LOG_TAG) << "pMiniBuffer->full() && (abs(pMiniBuffer->mean()) > 50): " << miniBuffer_.median() << "\n";
            hard_sync_ = true;
        }
        else if (cs::abs(age) > 500ms)
        {
            LOG(INFO, LOG_TAG) << "abs(age > 500): " << cs::abs(age).count() << "\n";
            hard_sync_ = true;
        }
        else if (shortBuffer_.full())
        {
            // No hard sync needed
            // Check if we need a samplerate correction (change playback speed (soft sync))
            auto miniMedian = miniBuffer_.median();
            if ((cs::usec(shortMedian_) > kCorrectionBegin) && (cs::usec(miniMedian) > cs::usec(50)) && (cs::usec(age) > cs::usec(50)))
            {
                double rate = (shortMedian_ / 100) * 0.00005;
                rate = 1.0 - std::min(rate, 0.0005);
                // LOG(INFO, LOG_TAG) << "Rate: " << rate << "\n";
                // we are late (age > 0), this means we are not playing fast enough
                // => the real sample rate seems to be lower, we have to drop some frames
                setRealSampleRate(format_.rate() * rate); // 0.9999);
            }
            else if ((cs::usec(shortMedian_) < -kCorrectionBegin) && (cs::usec(miniMedian) < -cs::usec(50)) && (cs::usec(age) < -cs::usec(50)))
            {
                double rate = (-shortMedian_ / 100) * 0.00005;
                rate = 1.0 + std::min(rate, 0.0005);
                // LOG(INFO, LOG_TAG) << "Rate: " << rate << "\n";
                // we are early (age > 0), this means we are playing too fast
                // => the real sample rate seems to be higher, we have to insert some frames
                setRealSampleRate(format_.rate() * rate); // 1.0001);
            }
        }

        updateBuffers(age.count());

        // update median_ and shortMedian_ and print sync stats
        if (now != lastUpdate_)
        {
            // log buffer stats
            lastUpdate_ = now;
            median_ = buffer_.median();
            shortMedian_ = shortBuffer_.median();
            LOG(DEBUG, "Stats") << "Chunk: " << age.count() / 100 << "\t" << miniBuffer_.median() / 100 << "\t" << shortMedian_ / 100 << "\t" << median_ / 100
                                << "\t" << buffer_.size() << "\t" << cs::duration<cs::msec>(outputBufferDacTime) << "\t" << frame_delta_ << "\n";
            frame_delta_ = 0;

#ifdef LOG_LATENCIES
            // log latencies
            std::array<uint8_t, 5> percents = {100, 99, 95, 50, 5};
            auto percentiles = latencies_.percentiles(percents);
            std::stringstream ss;
            for (std::size_t n = 0; n < percents.size(); ++n)
                ss << ((n > 0) ? ", " : "") << (int)percents[n] << "%: " << percentiles[n];
            LOG(DEBUG, "Latency") << ss.str() << "\n";
#endif
        }
        return (abs(cs::duration<cs::msec>(age)) < 500);
    }
    catch (const std::exception& e)
    {
        LOG(INFO, LOG_TAG) << "Exception: " << e.what() << "\n";
        hard_sync_ = true;
        return false;
    }
}


bool Stream::getPlayerChunkOrSilence(void* outputBuffer, const chronos::usec& outputBufferDacTime, uint32_t frames)
{
    bool result = getPlayerChunk(outputBuffer, outputBufferDacTime, frames);
    if (!result)
    {
        static utils::logging::TimeConditional cond(1s);
        LOG(DEBUG, LOG_TAG) << cond << "Failed to get chunk, returning silence\n";
        getSilentPlayerChunk(outputBuffer, frames);
    }
    return result;
}
