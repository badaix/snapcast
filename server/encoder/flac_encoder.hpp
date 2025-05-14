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
#include "encoder.hpp"

// 3rd party headers
#include "FLAC/stream_encoder.h"

// standard headers
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace encoder
{

class FlacEncoder : public Encoder
{
public:
    explicit FlacEncoder(const std::string& codecOptions = "");
    ~FlacEncoder() override;
    void encode(const msg::PcmChunk& chunk) override;
    std::string getAvailableOptions() const override;
    std::string getDefaultOptions() const override;
    std::string name() const override;

    FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples,
                                                  unsigned current_frame);

protected:
    void initEncoder() override;

    FLAC__StreamEncoder* encoder_;
    FLAC__StreamMetadata* metadata_[2];

    FLAC__int32* pcmBuffer_;
    int pcmBufferSize_;

    size_t encodedSamples_;
    std::shared_ptr<msg::PcmChunk> flacChunk_;
};

} // namespace encoder
