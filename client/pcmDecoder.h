#ifndef PCM_DECODER_H
#define PCM_DECODER_H
#include "decoder.h"


class PcmDecoder : public Decoder
{
public:
	PcmDecoder();
	virtual bool decode(msg::PcmChunk* chunk);
	virtual bool setHeader(msg::Header* chunk);
};


#endif


