#include "chunk.h"
#include <string.h>
#include <iostream>


Chunk::Chunk(size_t hz, size_t channels, size_t bitPerSample, WireChunk* _wireChunk) : wireChunk(_wireChunk), hz_(hz), channels_(channels), bytesPerSample_(bitPerSample/8), frameSize_(bytesPerSample_*channels_), idx(0)
{
}


Chunk::Chunk(size_t hz, size_t channels, size_t bitPerSample, size_t ms) : hz_(hz), channels_(channels), bytesPerSample_(bitPerSample/8), frameSize_(bytesPerSample_*channels_), idx(0)
{
	wireChunk = new WireChunk;
	wireChunk->length = hz*channels*bytesPerSample_*ms / 1000;
	wireChunk->payload = (char*)malloc(wireChunk->length);
}


Chunk::~Chunk()
{
	free(wireChunk->payload);
	delete wireChunk;
}


bool Chunk::isEndOfChunk() const
{
	return idx >= (wireChunk->length / frameSize_);
}



double Chunk::getDuration() const
{
//	std::cout << "len: " << wireChunk->length << ", channels: " << channels_ << ", bytesPerSample: " << bytesPerSample_ << ", hz: " << hz_ << "\n";
	return wireChunk->length / (channels_ * bytesPerSample_ * hz_ / 1000.);
}



int Chunk::read(void* outputBuffer, size_t frameCount) 
{
//std::cout << "read: " << frameCount << std::endl;
	int result = frameCount;
	if (idx + frameCount > wireChunk->length / frameSize_)
		result = (wireChunk->length / frameSize_) - idx;

	if (outputBuffer != NULL)
		memcpy(outputBuffer, wireChunk->payload + frameSize_*idx, frameSize_*result);

	idx += result;
	return result;
}


