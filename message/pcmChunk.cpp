/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <string.h>
#include <iostream>
#include "pcmChunk.h"
#include "common/log.h"


using namespace std;

namespace msg
{

PcmChunk::PcmChunk(const SampleFormat& sampleFormat, size_t ms) : WireChunk(sampleFormat.rate*sampleFormat.frameSize*ms / 1000), format(sampleFormat), idx_(0)
{
}


PcmChunk::PcmChunk(const PcmChunk& pcmChunk) : WireChunk(pcmChunk), format(pcmChunk.format), idx_(0)
{
}


PcmChunk::PcmChunk() : WireChunk(), idx_(0)
{
}


PcmChunk::~PcmChunk()
{
}



int PcmChunk::seek(int frames)
{
	if ((frames < 0) && (-frames > (int)idx_))
		frames = -idx_;
		
	idx_ += frames;
	if (idx_ > getFrameCount())
		idx_ = getFrameCount();

	return idx_;
}


int PcmChunk::readFrames(void* outputBuffer, size_t frameCount)
{
//logd << "read: " << frameCount << ", total: " << (wireChunk->length / format.frameSize) << ", idx: " << idx;// << std::endl;
	int result = frameCount;
	if (idx_ + frameCount > (payloadSize / format.frameSize))
		result = (payloadSize / format.frameSize) - idx_;

//logd << ", from: " << format.frameSize*idx << ", to: " << format.frameSize*idx + format.frameSize*result;
	if (outputBuffer != NULL)
		memcpy((char*)outputBuffer, (char*)(payload) + format.frameSize*idx_, format.frameSize*result);

	idx_ += result;
//logd << ", new idx: " << idx << ", result: " << result << ", wireChunk->length: " << wireChunk->length << ", format.frameSize: " << format.frameSize << "\n";//std::endl;
	return result;
}


}


