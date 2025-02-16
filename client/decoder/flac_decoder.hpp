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
#include "decoder.hpp"

// 3rd party headers
#include <FLAC/stream_decoder.h>

// standard headers
#include <memory>
#include <mutex>


namespace decoder
{


/// Cache internal decoder status
struct CacheInfo
{
    /// Reset current cache info
    void reset()
    {
        isCachedChunk_ = true;
        cachedBlocks_ = 0;
    }

    bool isCachedChunk_{true}; ///< is the current block cached
    size_t cachedBlocks_{0};   ///< number of cached blocks
    size_t sampleRate_{0};     ///< sample rate of the block
};


/// Flac decoder
class FlacDecoder : public Decoder
{
public:
    /// c'tor
    FlacDecoder();
    /// d'tor
    ~FlacDecoder() override;

    bool decode(msg::PcmChunk* chunk) override;
    SampleFormat setHeader(msg::CodecHeader* chunk) override;

    /// Flac internal cache info
    CacheInfo cacheInfo_;
    /// Last decoder error
    std::unique_ptr<FLAC__StreamDecoderErrorStatus> lastError_;

private:
    std::mutex mutex_;
};

} // namespace decoder
