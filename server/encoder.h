#ifndef ENCODER_H
#define ENCODER_H
#include "message/pcmChunk.h"
#include "message/headerMessage.h"
#include "message/sampleFormat.h"


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


