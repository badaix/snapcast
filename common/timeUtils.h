#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include "chunk.h"
#include <sys/time.h>
#include <chrono>

/*
typedef std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds> time_point_ms;


static inline time_point_ms timePoint(const Chunk* chunk)
{
	time_point_ms tp;
	return tp + std::chrono::seconds(chunk->tv_sec) + std::chrono::milliseconds(chunk->tv_usec / 1000) + std::chrono::milliseconds(chunk->idx / WIRE_CHUNK_MS_SIZE);
}



template<typename T, typename U>
static inline T getAge(const std::chrono::time_point<U>& time_point)
{
	return std::chrono::duration_cast<T>(std::chrono::high_resolution_clock::now() - time_point);
}



template<typename T>
static inline T getAge(const Chunk* chunk)
{
	return getAge<T>(timePoint(chunk));
}



inline static long getAge(const Chunk* chunk)
{
	return getAge<std::chrono::milliseconds>(chunk).count();
}



inline static long getAge(const time_point_ms& time_point)
{
	return getAge<std::chrono::milliseconds>(time_point).count();
}
*/ 


static void addMs(timeval& tv, int ms)
{
	if (ms < 0)
	{
		timeval t;
		t.tv_sec = -ms / 1000;
		t.tv_usec = (-ms % 1000) * 1000;
		timersub(&tv, &t, &tv);
		return;
	}
    tv.tv_usec += ms*1000;
    tv.tv_sec += (tv.tv_usec / 1000000);
    tv.tv_usec %= 1000000;
}



static long getTickCount()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec*1000 + now.tv_nsec / 1000000;
}


static long getuTickCount()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec*1000000 + now.tv_nsec / 1000;
}




#endif


