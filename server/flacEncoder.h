/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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
	FlacEncoder(const msg::SampleFormat& format);
	~FlacEncoder();
	virtual double encode(msg::PcmChunk* chunk);
	msg::Header* getHeaderChunk();

protected:
	void initEncoder();
	FLAC__StreamEncoder *encoder;
	FLAC__StreamMetadata *metadata[2];
//	virtual void progress_callback(FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate);
};


#endif


