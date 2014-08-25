#include "chunk.h"
#include <string.h>
#include <iostream>
#include "common/log.h"


Chunk::Chunk(const SampleFormat& sampleFormat, WireChunk* _wireChunk) : wireChunk(_wireChunk), format(sampleFormat), idx(0)
{
}


Chunk::Chunk(const SampleFormat& sampleFormat, size_t ms) : format(sampleFormat), idx(0)
{
//	format = sampleFormat;
	wireChunk = new WireChunk;
	wireChunk->length = format.rate*format.frameSize*ms / 1000;
	wireChunk->payload = (char*)malloc(wireChunk->length);
}


Chunk::~Chunk()
{
	free(wireChunk->payload);
	delete wireChunk;
}


bool Chunk::isEndOfChunk() const
{
	return idx >= getFrameCount();
}


double Chunk::getFrameCount() const
{
	return (wireChunk->length / format.frameSize);
}



double Chunk::getDuration() const
{
	return getFrameCount() / format.msRate();
}



double Chunk::getTimeLeft() const
{
	return (getFrameCount() - idx) / format.msRate();
}



int Chunk::seek(int frames)
{
	idx += frames;
	if (idx > getFrameCount())
		idx = getFrameCount();
	if (idx < 0)
		idx = 0;
	return idx;
}


int Chunk::read(void* outputBuffer, size_t frameCount) 
{
//logd << "read: " << frameCount << ", total: " << (wireChunk->length / format.frameSize) << ", idx: " << idx;// << std::endl;
	int result = frameCount;
	if (idx + frameCount > (wireChunk->length / format.frameSize))
		result = (wireChunk->length / format.frameSize) - idx;

//logd << ", from: " << format.frameSize*idx << ", to: " << format.frameSize*idx + format.frameSize*result;
	if (outputBuffer != NULL)
		memcpy((char*)outputBuffer, (char*)(wireChunk->payload) + format.frameSize*idx, format.frameSize*result);

	idx += result;
//logd << ", new idx: " << idx << ", result: " << result << ", wireChunk->length: " << wireChunk->length << ", format.frameSize: " << format.frameSize << "\n";//std::endl;
	return result;
}


