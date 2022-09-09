/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef MESSAGE_PCM_CHUNK_HPP
#define MESSAGE_PCM_CHUNK_HPP

// local headers
#include "common/sample_format.hpp"
#include "message.hpp"
#include "wire_chunk.hpp"

// standard headers
#include <chrono>


namespace msg
{

/**
 * Piece of PCM data with SampleFormat information
 * Has information about "when" recorded (start) and duration
 * frames can be read with "readFrames", which will also change the start time
 */
class PcmChunk : public WireChunk
{
public:
    PcmChunk(const SampleFormat& sampleFormat, uint32_t ms)
        : WireChunk((sampleFormat.rate() * ms / 1000) * sampleFormat.frameSize()), format(sampleFormat), idx_(0)
    {
    }

    PcmChunk() : WireChunk(), idx_(0)
    {
    }

    ~PcmChunk() override = default;

#if 0
    template <class Rep, class Period>
    int readFrames(void* outputBuffer, const std::chrono::duration<Rep, Period>& duration)
    {
        auto us = std::chrono::microseconds(duration).count();
        auto frames = (us * 48000) / std::micro::den;
        // return readFrames(outputBuffer, (us * 48000) / std::micro::den);
        return frames;
    }
#endif

    // std::unique_ptr<PcmChunk> consume(uint32_t frameCount)
    // {
    //     auto result = std::make_unique<PcmChunk>(format, 0);
    //     if (frameCount * format.frameSize() > payloadSize)
    //         frameCount = payloadSize / format.frameSize();
    //     result->payload = payload;
    //     result->payloadSize = frameCount * format.frameSize();
    //     payloadSize -= result->payloadSize;
    //     payload = (char*)realloc(payload + result->payloadSize, payloadSize);
    //     // payload += result->payloadSize;
    //     return result;
    // }

    int readFrames(void* outputBuffer, uint32_t frameCount)
    {
        // logd << "read: " << frameCount << ", total: " << (wireChunk->length / format.frameSize()) << ", idx: " << idx;// << std::endl;
        int result = frameCount;
        if (idx_ + frameCount > (payloadSize / format.frameSize()))
            result = (payloadSize / format.frameSize()) - idx_;

        // logd << ", from: " << format.frameSize()*idx << ", to: " << format.frameSize()*idx + format.frameSize()*result;
        if (outputBuffer != nullptr)
            memcpy((char*)outputBuffer, (char*)(payload) + format.frameSize() * idx_, format.frameSize() * result);

        idx_ += result;
        // logd << ", new idx: " << idx << ", result: " << result << ", wireChunk->length: " << wireChunk->length << ", format.frameSize(): " <<
        // format.frameSize() << "\n";
        return result;
    }

    int seek(int frames)
    {
        if ((frames < 0) && (-frames > static_cast<int>(idx_)))
            frames = -static_cast<int>(idx_);

        idx_ += frames;
        if (idx_ > getFrameCount())
            idx_ = getFrameCount();

        return idx_;
    }

    chronos::time_point_clk start() const override
    {
        return chronos::time_point_clk(chronos::sec(timestamp.sec) + chronos::usec(timestamp.usec) +
                                       chronos::usec(static_cast<chronos::usec::rep>(1000000. * ((double)idx_ / (double)format.rate()))));
    }

    inline chronos::time_point_clk end() const
    {
        return start() + durationLeft<chronos::usec>();
    }

    template <typename T>
    inline T duration() const
    {
        return std::chrono::duration_cast<T>(chronos::nsec(static_cast<chronos::nsec::rep>(1000000 * getFrameCount() / format.msRate())));
    }

    // void append(const PcmChunk& chunk)
    // {
    //     auto newSize = payloadSize + chunk.payloadSize;
    //     payload = (char*)realloc(payload, newSize);
    //     memcpy(payload + payloadSize, chunk.payload, chunk.payloadSize);
    //     payloadSize = newSize;
    // }

    double durationMs() const
    {
        return static_cast<double>(getFrameCount()) / format.msRate();
    }

    template <typename T>
    inline T durationLeft() const
    {
        return std::chrono::duration_cast<T>(chronos::nsec(static_cast<chronos::nsec::rep>(1000000 * (getFrameCount() - idx_) / format.msRate())));
    }

    inline bool isEndOfChunk() const
    {
        return idx_ >= getFrameCount();
    }

    inline uint32_t getFrameCount() const
    {
        return (payloadSize / format.frameSize());
    }

    inline uint32_t getSampleCount() const
    {
        return (payloadSize / format.sampleSize());
    }

    SampleFormat format;

private:
    uint32_t idx_ = 0;
};
} // namespace msg

#endif
