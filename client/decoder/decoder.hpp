/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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
#include <mutex>

namespace decoder
{

class Decoder
{
public:
    Decoder(){};
    virtual ~Decoder() = default;

    virtual bool decode(msg::PcmChunk* chunk) = 0;
    virtual SampleFormat setHeader(msg::CodecHeader* chunk) = 0;

protected:
    std::mutex mutex_;
};

} // namespace decoder
