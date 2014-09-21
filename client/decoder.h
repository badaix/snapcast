#ifndef DECODER_H
#define DECODER_H
#include "message/pcmChunk.h"
#include "message/headerMessage.h"

class Decoder
{
public:
	Decoder() {};
	virtual ~Decoder() {};
	virtual bool decode(PcmChunk* chunk) = 0;
	virtual bool setHeader(HeaderMessage* chunk) = 0;
};


#endif


