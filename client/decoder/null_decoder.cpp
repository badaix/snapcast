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

// prototype/interface header file
#include "null_decoder.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/endian.hpp"
#include "common/snap_exception.hpp"

// 3rd party headers

// standard headers


namespace decoder
{

NullDecoder::NullDecoder() : Decoder()
{
}


bool NullDecoder::decode(msg::PcmChunk* /*chunk*/)
{
    return true;
}


SampleFormat NullDecoder::setHeader(msg::CodecHeader* chunk)
{
    std::ignore = chunk;
    return {48000, 16, 2};
}

} // namespace decoder
