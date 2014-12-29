#ifndef ENCODER_H
#define ENCODER_H
#include "message/pcmChunk.h"
#include "message/header.h"
#include "message/sampleFormat.h"


class Encoder
{
public:
	Encoder(const msg::SampleFormat& format) : sampleFormat(format), headerChunk(NULL)
	{
	}

	virtual ~Encoder()
	{
		if (headerChunk != NULL)
			delete headerChunk;
	}

	virtual double encode(msg::PcmChunk* chunk) = 0;

	virtual msg::Header* getHeader()
	{
		return headerChunk;
	}

protected:
	msg::SampleFormat sampleFormat;
	msg::Header* headerChunk;
};


#endif


