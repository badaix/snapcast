#ifndef DECODER_H
#define DECODER_H
#include "common/pcmChunk.h"
#include "common/headerMessage.h"

class Decoder
{
public:
	Decoder() {};
	virtual ~Decoder() {};
	virtual bool decode(PcmChunk* chunk) = 0;
	virtual bool setHeader(HeaderMessage* chunk) = 0;
};


#endif


