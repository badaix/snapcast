#ifndef ENCODER_H
#define ENCODER_H
#include "common/pcmChunk.h"
#include "common/headerMessage.h"
#include "common/sampleFormat.h"


class Encoder
{
public:
	Encoder(const SampleFormat& format) : sampleFormat(format), headerChunk(NULL)
	{
	}

	virtual ~Encoder()
	{
		if (headerChunk != NULL)
			delete headerChunk;
	}

	virtual double encode(PcmChunk* chunk) = 0;

	virtual HeaderMessage* getHeader()
	{
		return headerChunk;
	}

protected:
	SampleFormat sampleFormat;
	HeaderMessage* headerChunk;
};


#endif


