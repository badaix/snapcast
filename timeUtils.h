#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include "chunk.h"
#include <sys/time.h>

static std::string timeToStr(const timeval& timestamp)
{
	char tmbuf[64], buf[64];
	struct tm *nowtm;
	time_t nowtime;
	nowtime = timestamp.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
	snprintf(buf, sizeof buf, "%s.%06d", tmbuf, (int)timestamp.tv_usec);
	return buf;
}


static timeval getTimeval(const Chunk* chunk)
{
	timeval ts;
	ts.tv_sec = chunk->tv_sec;
	ts.tv_usec = chunk->tv_usec;
	return ts;
}


static void setTimeval(Chunk* chunk, timeval tv)
{
	chunk->tv_sec = tv.tv_sec;
	chunk->tv_usec = tv.tv_usec;
}


static std::string chunkTime(const Chunk* chunk)
{
	return timeToStr(getTimeval(chunk));
}


static long diff_ms(const timeval& t1, const timeval& t2)
{
    return (((t1.tv_sec - t2.tv_sec) * 1000000) + 
            (t1.tv_usec - t2.tv_usec))/1000;
}


static long getAge(const Chunk* chunk)
{
	timeval now;
	gettimeofday(&now, NULL);
	return diff_ms(now, getTimeval(chunk));
}
 

static long getAge(const timeval& tv)
{
	timeval now;
	gettimeofday(&now, NULL);
	return diff_ms(now, tv);
}
 

static long getTickCount()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec*1000 + now.tv_nsec / 1000000;
}


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


static void addMs(Chunk* chunk, int ms)
{
	timeval tv = getTimeval(chunk);
	addMs(tv, ms);
	chunk->tv_sec = tv.tv_sec;
	chunk->tv_usec = tv.tv_usec;
}



#endif


