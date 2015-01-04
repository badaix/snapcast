#ifndef FLAC_DECODER_H
#define FLAC_DECODER_H
#include "decoder.h"


class FlacDecoder : public Decoder
{
public:
	FlacDecoder();
	virtual ~FlacDecoder();
	virtual bool decode(msg::PcmChunk* chunk);
	virtual bool setHeader(msg::Header* chunk);
};


#endif


