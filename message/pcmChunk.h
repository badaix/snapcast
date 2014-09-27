#ifndef PCM_CHUNK_H
#define PCM_CHUNK_H

#include <chrono>
#include "message.h"
#include "wireChunk.h"
#include "sampleFormat.h"
#include "common/timeDefs.h"



class PcmChunk : public WireChunk
{
public:
	PcmChunk(const SampleFormat& sampleFormat, size_t ms);
	PcmChunk();
	~PcmChunk();

	int readFrames(void* outputBuffer, size_t frameCount);
	int seek(int frames);

	inline chronos::time_point_hrc start() const
	{
		return chronos::time_point_hrc(
				chronos::sec(timestamp.sec) + 
				chronos::usec(timestamp.usec) + 
				chronos::usec((chronos::usec::rep)(1000000. * ((double)idx / (double)format.rate)))
				);
	}

	inline chronos::time_point_hrc end() const
	{
		return start() + durationLeft<chronos::usec>();
	}
	
	template<typename T>
	inline T duration() const
	{
		return std::chrono::duration_cast<T>(chronos::nsec((chronos::nsec::rep)(1000000 * getFrameCount() / format.msRate())));
	}

	template<typename T>
	inline T durationLeft() const
	{
		return std::chrono::duration_cast<T>(chronos::nsec((chronos::nsec::rep)(1000000 * (getFrameCount() - idx) / format.msRate())));
	}

	inline bool isEndOfChunk() const
	{
		return idx >= getFrameCount();
	}

	inline size_t getFrameCount() const
	{
		return (payloadSize / format.frameSize);
	}

	SampleFormat format;

private:
//	SampleFormat format_;
	uint32_t idx;
};



#endif


