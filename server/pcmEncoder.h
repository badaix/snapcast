#ifndef PCM_ENCODER_H
#define PCM_ENCODER_H
#include "encoder.h"


class PcmEncoder : public Encoder
{
public:
	PcmEncoder(const msg::SampleFormat& format);
	virtual double encode(msg::PcmChunk* chunk);
};


#endif


