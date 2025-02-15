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
#include "common/message/pcm_chunk.hpp"
#include "common/sample_format.hpp"

// 3rd party headers
#ifdef HAS_SOXR
#include <soxr.h>
#endif

// standard headers
#include <vector>


/// Resampler
class Resampler
{
public:
    /// c'tor to resample from @p in_format to @p out_format
    Resampler(const SampleFormat& in_format, const SampleFormat& out_format);
    /// d'tor
    virtual ~Resampler();

    /// @return resampled @p chunk
    std::shared_ptr<msg::PcmChunk> resample(std::shared_ptr<msg::PcmChunk> chunk);
    /// @return resampled @p chunk
    std::shared_ptr<msg::PcmChunk> resample(const msg::PcmChunk& chunk);
    /// @return if resampling is needed (in_format != out_format)
    bool resamplingNeeded() const;

private:
    std::vector<char> resample_buffer_;
    SampleFormat in_format_;
    SampleFormat out_format_;
#ifdef HAS_SOXR
    soxr_t soxr_{nullptr};
#endif
};
