#ifndef DECODER_H
#define DECODER_H
#include "common/message.h"

class Decoder
{
public:
	Decoder();
	virtual bool decode(PcmChunk* chunk) = 0;
};


#endif


