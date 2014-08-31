#ifndef ENCODER_H
#define ENCODER_H
#include "common/chunk.h"
#include "common/sampleFormat.h"


class Encoder
{
public:
	Encoder(const SampleFormat& format) : sampleFormat(format)
	{
	}

	virtual double encode(Chunk* chunk) = 0;
	virtual WireChunk* getHeader()
	{
		return NULL;
	}

protected:
	SampleFormat sampleFormat;
};


#endif


