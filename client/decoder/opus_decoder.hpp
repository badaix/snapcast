/***
        This file is part of snapcast
        Copyright (C) 2015  Hannes Ellinger
        Copyright (C) 2016-2024  Johannes Pohl

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
#include "decoder/decoder.hpp"

// 3rd party headers
#ifdef MACOS
#include <opus.h>
#else
#include <opus/opus.h>
#endif

// standard headers
#include <vector>


namespace decoder
{

class OpusDecoder : public Decoder
{
public:
    OpusDecoder();
    ~OpusDecoder();
    bool decode(msg::PcmChunk* chunk) override;
    SampleFormat setHeader(msg::CodecHeader* chunk) override;

private:
    ::OpusDecoder* dec_;
    std::vector<opus_int16> pcm_;
    SampleFormat sample_format_;
};

} // namespace decoder
