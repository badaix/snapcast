#ifndef ENCODER_H
#define ENCODER_H
#include "common/chunk.h"


class Encoder
{
public:
	Encoder();
	virtual double encode(Chunk* chunk) = 0;
};


#endif


