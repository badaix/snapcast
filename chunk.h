#ifndef CHUNK_H
#define CHUNK_H

#include <chrono>

#define SAMPLE_RATE (48000)
//#define SAMPLE_BIT (16)
#define CHANNELS (2)

#define WIRE_CHUNK_MS (50)
#define WIRE_CHUNK_SIZE ((SAMPLE_RATE*CHANNELS*WIRE_CHUNK_MS)/1000)
#define WIRE_CHUNK_MS_SIZE ((SAMPLE_RATE*CHANNELS)/1000)
#define SAMPLE_SIZE (CHANNELS)

#define PLAYER_CHUNK_MS (10)
#define PLAYER_CHUNK_SIZE ((SAMPLE_RATE*CHANNELS*PLAYER_CHUNK_MS)/1000)
#define PLAYER_CHUNK_MS_SIZE ((SAMPLE_RATE*CHANNELS)/1000)
#define FRAMES_PER_BUFFER  ((SAMPLE_RATE*PLAYER_CHUNK_MS)/1000)


typedef std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds> time_point_ms;


struct WireChunk
{
	int32_t tv_sec;
	int32_t tv_usec;
	int16_t payload[WIRE_CHUNK_SIZE];
};



class Chunk
{
public:
	Chunk(WireChunk* _wireChunk);
	~Chunk();

	int read(short* _outputBuffer, int _count);
	bool isEndOfChunk();

	inline time_point_ms timePoint()
	{
		time_point_ms tp;
		return tp + std::chrono::seconds(wireChunk->tv_sec) + std::chrono::milliseconds(wireChunk->tv_usec / 1000) + std::chrono::milliseconds(idx / WIRE_CHUNK_MS_SIZE);
	}

	template<typename T>
	inline T getAge()
	{
		return getAge<T>(timePoint());
	}

	inline long getAge()
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

private:
	int32_t idx;
	WireChunk* wireChunk;
};



#endif


