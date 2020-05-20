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

#ifndef FLAC_DECODER_H
#define FLAC_DECODER_H

#include "decoder.hpp"

#include <FLAC/stream_decoder.h>
#include <atomic>
#include <memory>

namespace decoder
{

struct CacheInfo
{
    CacheInfo() : sampleRate_(0)
    {
        reset();
    }

    void reset()
    {
        isCachedChunk_ = true;
        cachedBlocks_ = 0;
    }

    bool isCachedChunk_;
    size_t cachedBlocks_;
    size_t sampleRate_;
};


class FlacDecoder : public Decoder
{
public:
    FlacDecoder();
    ~FlacDecoder() override;
    bool decode(msg::PcmChunk* chunk) override;
    SampleFormat setHeader(msg::CodecHeader* chunk) override;

    CacheInfo cacheInfo_;
    std::unique_ptr<FLAC__StreamDecoderErrorStatus> lastError_;
};

} // namespace decoder

#endif
