#include "pcmDecoder.h"

PcmDecoder::PcmDecoder() : Decoder()
{
}


bool PcmDecoder::decode(msg::PcmChunk* chunk)
{
	return true;
}


bool PcmDecoder::setHeader(msg::Header* chunk)
{
	return true;
}




