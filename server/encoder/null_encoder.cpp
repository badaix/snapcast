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

// prototype/interface header file
#include "null_encoder.hpp"

// local headers
#include "common/aixlog.hpp"

// standard headers
#include <memory>


namespace encoder
{

static constexpr auto LOG_TAG = "NullEnc";

NullEncoder::NullEncoder(std::string codecOptions) : Encoder(std::move(codecOptions))
{
    headerChunk_ = std::make_shared<msg::CodecHeader>("null");
}


void NullEncoder::encode(const msg::PcmChunk& chunk)
{
    std::ignore = chunk;
}


void NullEncoder::initEncoder()
{
    LOG(INFO, LOG_TAG) << "Init\n";
}


std::string NullEncoder::name() const
{
    return "null";
}

} // namespace encoder
