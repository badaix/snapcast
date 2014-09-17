#include "pcmDecoder.h"

PcmDecoder::PcmDecoder() : Decoder()
{
}


bool PcmDecoder::decode(PcmChunk* chunk)
{
    /*	WireChunk* wireChunk = chunk->wireChunk;
    	for (size_t n=0; n<wireChunk->length; ++n)
    		wireChunk->payload[n] *= 1;
    */
    return true;
}


bool PcmDecoder::setHeader(HeaderMessage* chunk)
{
    return true;
}




