#ifndef PCM_ENCODER_H
#define PCM_ENCODER_H
#include "common/chunk.h"


class PcmEncoder
{
public:
	PcmEncoder();
	void encode(Chunk* chunk);
};


#endif


