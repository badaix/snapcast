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

#ifndef STREAM_H
#define STREAM_H

#include "common/queue.h"
#include "common/sample_format.hpp"
#include "double_buffer.hpp"
#include "message/message.hpp"
#include "message/pcm_chunk.hpp"
#include <deque>
#include <memory>
#include <soxr.h>


/// Time synchronized audio stream
/**
 * Queue with PCM data.
 * Returns "online" server-time-synchronized PCM data
 */
class Stream
{
public:
    Stream(const SampleFormat& in_format, const SampleFormat& out_format);
    virtual ~Stream();

    /// Adds PCM data to the queue
    void addChunk(std::unique_ptr<msg::PcmChunk> chunk);
    void clearChunks();

    /// Get PCM data, which will be played out in "outputBufferDacTime" time
    /// frame = (num_channels) * (1 sample in bytes) = (2 channels) * (2 bytes (16 bits) per sample) = 4 bytes (32 bits)
    bool getPlayerChunk(void* outputBuffer, const chronos::usec& outputBufferDacTime, unsigned long framesPerBuffer);

    /// "Server buffer": playout latency, e.g. 1000ms
    void setBufferLen(size_t bufferLenMs);

    const SampleFormat& getFormat() const
    {
        return format_;
    }

    bool waitForChunk(size_t ms) const;

private:
    chronos::time_point_clk getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long frames);
    chronos::time_point_clk getNextPlayerChunk(void* outputBuffer, const chronos::usec& timeout, unsigned long frames, long framesCorrection);
    chronos::time_point_clk getSilentPlayerChunk(void* outputBuffer, unsigned long frames);

    void updateBuffers(int age);
    void resetBuffers();
    void setRealSampleRate(double sampleRate);

    SampleFormat format_;
    SampleFormat in_format_;

    Queue<std::shared_ptr<msg::PcmChunk>> chunks_;
    DoubleBuffer<chronos::usec::rep> miniBuffer_;
    DoubleBuffer<chronos::usec::rep> buffer_;
    DoubleBuffer<chronos::usec::rep> shortBuffer_;
    std::shared_ptr<msg::PcmChunk> chunk_;

    int median_;
    int shortMedian_;
    time_t lastUpdate_;
    unsigned long playedFrames_;
    long correctAfterXFrames_;
    chronos::msec bufferMs_;

    soxr_t soxr_;
    std::vector<char> resample_buffer_;
    int frame_delta_;
    // int64_t next_us_;

    bool hard_sync_;
};



#endif
