#ifndef CHUNK_H
#define CHUNK_H

#include <chrono>
#include <cstdlib>
#include "common/sampleFormat.h"


typedef std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds> time_point_ms;


struct WireChunk
{
	int32_t tv_sec;
	int32_t tv_usec;
	uint32_t length;
	char* payload;
};



class Chunk
{
public:
	Chunk(const SampleFormat& sampleFormat, WireChunk* _wireChunk);
	Chunk(const SampleFormat& sampleFormat, size_t ms);
	~Chunk();

	static inline size_t getHeaderSize()
	{
		return sizeof(WireChunk::tv_sec) + sizeof(WireChunk::tv_usec) + sizeof(WireChunk::length);
	}

	int read(void* outputBuffer, size_t frameCount);
	bool isEndOfChunk() const;

	inline time_point_ms timePoint() const
	{
		time_point_ms tp;
		std::chrono::milliseconds::rep relativeIdxTp = ((double)idx / ((double)format.rate/1000.));
		return 
			tp + 
			std::chrono::seconds(wireChunk->tv_sec) + 
			std::chrono::milliseconds(wireChunk->tv_usec / 1000) + 
			std::chrono::milliseconds(relativeIdxTp);
	}

	template<typename T>
	inline T getAge() const
	{
		return getAge<T>(timePoint());
	}

	inline long getAge() const
	{
		return getAge<std::chrono::milliseconds>().count();
	}

	inline static long getAge(const time_point_ms& time_point)
	{
		return getAge<std::chrono::milliseconds>(time_point).count();
	}

	template<typename T, typename U>
	static inline T getAge(const std::chrono::time_point<U>& time_point)
	{
		return std::chrono::duration_cast<T>(std::chrono::high_resolution_clock::now() - time_point);
	}

	double getDuration() const;

	WireChunk* wireChunk;
	const SampleFormat& format;

private:
	SampleFormat format_;
	uint32_t idx;
};



#endif


