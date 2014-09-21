#include "pcmChunk.h"
#include <string.h>
#include <iostream>
#include "common/log.h"


using namespace std;


PcmChunk::PcmChunk(const SampleFormat& sampleFormat, size_t ms) : WireChunk(sampleFormat.rate*sampleFormat.frameSize*ms / 1000), format(sampleFormat), idx(0)
{
}


PcmChunk::PcmChunk() : WireChunk(), idx(0)
{
}


PcmChunk::~PcmChunk()
{
}


bool PcmChunk::isEndOfChunk() const
{
	return idx >= getFrameCount();
}


double PcmChunk::getFrameCount() const
{
	return (payloadSize / format.frameSize);
}



double PcmChunk::getDuration() const
{
	return getFrameCount() / format.msRate();
}



double PcmChunk::getTimeLeft() const
{
	return (getFrameCount() - idx) / format.msRate();
}



int PcmChunk::seek(int frames)
{
	idx += frames;
	if (idx > getFrameCount())
		idx = getFrameCount();
	if (idx < 0)
		idx = 0;
	return idx;
}


int PcmChunk::readFrames(void* outputBuffer, size_t frameCount)
{
//logd << "read: " << frameCount << ", total: " << (wireChunk->length / format.frameSize) << ", idx: " << idx;// << std::endl;
	int result = frameCount;
	if (idx + frameCount > (payloadSize / format.frameSize))
		result = (payloadSize / format.frameSize) - idx;

//logd << ", from: " << format.frameSize*idx << ", to: " << format.frameSize*idx + format.frameSize*result;
	if (outputBuffer != NULL)
		memcpy((char*)outputBuffer, (char*)(payload) + format.frameSize*idx, format.frameSize*result);

	idx += result;
//logd << ", new idx: " << idx << ", result: " << result << ", wireChunk->length: " << wireChunk->length << ", format.frameSize: " << format.frameSize << "\n";//std::endl;
	return result;
}


