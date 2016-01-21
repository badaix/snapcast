/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#ifndef FLAC_ENCODER_H
#define FLAC_ENCODER_H
#include "encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"


class FlacEncoder : public Encoder
{
public:
    FlacEncoder(const std::string& codecOptions = "");
    ~FlacEncoder();
    virtual void encode(const msg::PcmChunk* chunk);
   	virtual std::string getAvailableOptions() const;
	virtual std::string getDefaultOptions() const;
	virtual std::string name() const;

    FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame);

protected:
    virtual void initEncoder();

    FLAC__StreamEncoder *encoder_;
    FLAC__StreamMetadata *metadata_[2];

    FLAC__int32 *pcmBuffer_;
    int pcmBufferSize_;

    msg::PcmChunk* flacChunk_;
    size_t encodedSamples_;
};


#endif


