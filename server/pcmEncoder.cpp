#include "pcmEncoder.h"

PcmEncoder::PcmEncoder(const msg::SampleFormat& format) : Encoder(format)
{
	headerChunk = new msg::Header("pcm");
}


double PcmEncoder::encode(msg::PcmChunk* chunk)
{
	return chunk->duration<chronos::msec>().count();
}




