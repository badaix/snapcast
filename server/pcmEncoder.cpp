#include "pcmEncoder.h"

PcmEncoder::PcmEncoder(const SampleFormat& format) : Encoder(format)
{
}


double PcmEncoder::encode(Chunk* chunk)
{
/*	WireChunk* wireChunk = chunk->wireChunk;
	for (size_t n=0; n<wireChunk->length; ++n)
		wireChunk->payload[n] *= 1;
*/
	return chunk->getDuration();
}




