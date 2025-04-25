/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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

#pragma once

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
    /// c'tor, construct from @p sample_format with duration @p ms
    PcmChunk(const SampleFormat& sample_format, uint32_t ms) : WireChunk((sample_format.rate() * ms / 1000) * sample_format.frameSize()), format(sample_format)
    {
    }

    /// c'tor
    PcmChunk() : WireChunk()
    {
    }

    /// d'tor
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

    /// Read the next @p frame_count frames into @p output_buffer, update the internal read position
    /// @return number of frames copied to @p output_buffer
    int readFrames(void* output_buffer, uint32_t frame_count)
    {
        // logd << "read: " << frameCount << ", total: " << (wireChunk->length / format.frameSize()) << ", idx: " << idx;// << "\n";
        int result = frame_count;
        if (idx_ + frame_count > (payloadSize / format.frameSize()))
            result = (payloadSize / format.frameSize()) - idx_;

        // logd << ", from: " << format.frameSize()*idx << ", to: " << format.frameSize()*idx + format.frameSize()*result;
        if (output_buffer != nullptr)
            memcpy(static_cast<char*>(output_buffer), static_cast<char*>(payload) + format.frameSize() * idx_, format.frameSize() * result);

        idx_ += result;
        // logd << ", new idx: " << idx << ", result: " << result << ", wireChunk->length: " << wireChunk->length << ", format.frameSize(): " <<
        // format.frameSize() << "\n";
        return result;
    }

    /// seek @p frames forward or backward
    /// @return the new read position
    int seek(int frames)
    {
        if ((frames < 0) && (-frames > static_cast<int>(idx_)))
            frames = -static_cast<int>(idx_);

        idx_ += frames;
        if (idx_ > getFrameCount())
            idx_ = getFrameCount();

        return idx_;
    }

    /// @return start time of the current frame
    chronos::time_point_clk start() const override
    {
        return chronos::time_point_clk(chronos::sec(timestamp.sec) + chronos::usec(timestamp.usec) +
                                       chronos::usec(static_cast<chronos::usec::rep>(1000000. * ((double)idx_ / (double)format.rate()))));
    }

    /// @return time of the last frame
    inline chronos::time_point_clk end() const
    {
        return start() + durationLeft<chronos::usec>();
    }

    /// @return duration of this chunk
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

    /// Set the @p frame_count, reserve memory
    void setFrameCount(int frame_count)
    {
        auto new_size = format.frameSize() * frame_count;
        payload = static_cast<char*>(realloc(payload, new_size));
        payloadSize = new_size;
    }

    /// @return duration of this chunk in [ms]
    double durationMs() const
    {
        return static_cast<double>(getFrameCount()) / format.msRate();
    }

    /// @return time left, starting from the read pointer
    template <typename T>
    inline T durationLeft() const
    {
        return std::chrono::duration_cast<T>(chronos::nsec(static_cast<chronos::nsec::rep>(1000000 * (getFrameCount() - idx_) / format.msRate())));
    }

    /// @return true if the read pointer is at the end
    inline bool isEndOfChunk() const
    {
        return idx_ >= getFrameCount();
    }

    /// @return number of frames
    inline uint32_t getFrameCount() const
    {
        return (payloadSize / format.frameSize());
    }

    /// @return number of samples
    inline uint32_t getSampleCount() const
    {
        return (payloadSize / format.sampleSize());
    }

    /// Sample format of this chunk
    SampleFormat format;

private:
    /// current read position (frame idx)
    uint32_t idx_ = 0;
};

} // namespace msg
