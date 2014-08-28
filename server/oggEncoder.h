#ifndef OGG_ENCODER_H
#define OGG_ENCODER_H
#include "encoder.h"


class OggEncoder
{
public:
	OggEncoder();
	virtual void encode(Chunk* chunk);
};


#endif


