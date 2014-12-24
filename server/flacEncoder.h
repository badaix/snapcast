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
	FlacEncoder(const SampleFormat& format);
	virtual double encode(PcmChunk* chunk);

protected:
	void initEncoder();
//	virtual void progress_callback(FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate);
};


#endif


