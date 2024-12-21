/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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
#include "common/message/codec_header.hpp"
#include "common/message/pcm_chunk.hpp"
#include "common/sample_format.hpp"

// 3rd party headers

// standard headers

namespace decoder
{

/// Base class for an audio decoder
class Decoder
{
public:
    /// c'tor
    Decoder() = default;
    /// d'tor
    virtual ~Decoder() = default;

    /// decode encoded data stored in @p chunk, and write decoded data back into @p chunk
    /// return true, if data could be decoded and written back into @p chunk
    virtual bool decode(msg::PcmChunk* chunk) = 0;

    /// Set the codec header, stored in @p chunk.
    /// The CodecHeader is sent to every newly connected streaming client as first audio message.
    /// @return the sampleformat, decoded from the header
    virtual SampleFormat setHeader(msg::CodecHeader* chunk) = 0;
};

} // namespace decoder
