#ifndef DECODER_H
#define DECODER_H
#include "message/pcmChunk.h"
#include "message/header.h"

class Decoder
{
public:
	Decoder() {};
	virtual ~Decoder() {};
	virtual bool decode(msg::PcmChunk* chunk) = 0;
	virtual bool setHeader(msg::Header* chunk) = 0;
};


#endif


