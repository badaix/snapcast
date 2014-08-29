#ifndef PCM_DECODER_H
#define PCM_DECODER_H
#include "decoder.h"


class PcmDecoder
{
public:
	PcmDecoder();
	virtual bool decode(Chunk* chunk);
};


#endif


