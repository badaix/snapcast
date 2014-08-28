#ifndef ENCODER_H
#define ENCODER_H
#include "common/chunk.h"


class Encoder
{
public:
	Encoder();
	virtual void encode(Chunk* chunk) = 0;
};


#endif


