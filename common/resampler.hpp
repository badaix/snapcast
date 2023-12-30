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

#ifndef RESAMPLER_HPP
#define RESAMPLER_HPP

#include "common/message/pcm_chunk.hpp"
#include "common/sample_format.hpp"
#include <deque>
#include <vector>
#ifdef HAS_SOXR
#include <soxr.h>
#endif


class Resampler
{
public:
    Resampler(const SampleFormat& in_format, const SampleFormat& out_format);
    virtual ~Resampler();

    // std::shared_ptr<msg::PcmChunk> resample(std::shared_ptr<msg::PcmChunk> chunk, chronos::usec duration);
    std::shared_ptr<msg::PcmChunk> resample(std::shared_ptr<msg::PcmChunk> chunk);
    std::shared_ptr<msg::PcmChunk> resample(const msg::PcmChunk& chunk);
    bool resamplingNeeded() const;

private:
    std::vector<char> resample_buffer_;
    // std::unique_ptr<msg::PcmChunk> resampled_chunk_;
    SampleFormat in_format_;
    SampleFormat out_format_;
#ifdef HAS_SOXR
    soxr_t soxr_{nullptr};
#endif
};

#endif
