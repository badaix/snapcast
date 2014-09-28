#include "pcmEncoder.h"

PcmEncoder::PcmEncoder(const SampleFormat& format) : Encoder(format)
{
	headerChunk = new HeaderMessage("pcm");
}


double PcmEncoder::encode(PcmChunk* chunk)
{
	return chunk->duration<chronos::msec>().count();
}




