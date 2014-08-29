#ifndef DECODER_H
#define DECODER_H
#include "common/chunk.h"


class Decoder
{
public:
	Decoder();
	virtual bool decode(Chunk* chunk) = 0;
};


#endif


