#ifndef PCM_ENCODER_H
#define PCM_ENCODER_H
#include "encoder.h"


class PcmEncoder
{
public:
	PcmEncoder();
	virtual double encode(Chunk* chunk);
};


#endif


