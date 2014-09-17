#ifndef ENCODER_H
#define ENCODER_H
#include "common/pcmChunk.h"
#include "common/headerMessage.h"
#include "common/sampleFormat.h"


class Encoder
{
public:
    Encoder(const SampleFormat& format) : sampleFormat(format)
    {
    }

    virtual double encode(PcmChunk* chunk) = 0;
    virtual HeaderMessage* getHeader()
    {
        return NULL;
    }

protected:
    SampleFormat sampleFormat;
};


#endif


