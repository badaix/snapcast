#include "chunk.h"
#include <string.h>
#include <iostream>


Chunk::Chunk(size_t hz, size_t channels, size_t bitPerSample, WireChunk* _wireChunk) : wireChunk(_wireChunk), hz_(hz), channels_(channels), bytesPerSample_(bitPerSample/8), idx(0)
{
	frameSize_ = bytesPerSample_*channels_;
}


Chunk::Chunk(size_t hz, size_t channels, size_t bitPerSample, size_t ms) : hz_(hz), channels_(channels), bytesPerSample_(bitPerSample/8), idx(0)
{
	frameSize_ = bytesPerSample_*channels_;
	wireChunk = new WireChunk;
	wireChunk->length = hz*frameSize_*ms / 1000;
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
	return wireChunk->length / (frameSize_ * hz_ / 1000.);
}



int Chunk::read(void* outputBuffer, size_t frameCount) 
{
//std::cout << "read: " << frameCount << ", total: " << (wireChunk->length / frameSize_) << ", idx: " << idx;// << std::endl;
	int result = frameCount;
	if (idx + frameCount > (wireChunk->length / frameSize_))
		result = (wireChunk->length / frameSize_) - idx;

//std::cout << ", from: " << frameSize_*idx << ", to: " << frameSize_*idx + frameSize_*result;
	if (outputBuffer != NULL)
		memcpy((char*)outputBuffer, (char*)(wireChunk->payload) + frameSize_*idx, frameSize_*result);

	idx += result;
//std::cout << ", new idx: " << idx << ", result: " << result << std::endl;
	return result;
}


