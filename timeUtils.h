#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include "chunk.h"

std::string timeToStr(const timeval& timestamp)
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


std::string chunkTime(const Chunk& chunk)
{
	timeval ts;
	ts.tv_sec = chunk.tv_sec;
	ts.tv_usec = chunk.tv_usec;
	return timeToStr(ts);
}


int diff_ms(const timeval& t1, const timeval& t2)
{
    return (((t1.tv_sec - t2.tv_sec) * 1000000) + 
            (t1.tv_usec - t2.tv_usec))/1000;
}


int getAge(const Chunk& chunk)
{
	timeval now;
	gettimeofday(&now, NULL);
	timeval ts;
	ts.tv_sec = chunk.tv_sec;
	ts.tv_usec = chunk.tv_usec;
	return diff_ms(now, ts);
}
 

long getTickCount()
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec*1000 + now.tv_nsec / 1000000;
}


void addMs(timeval& tv, int ms)
{
    tv.tv_usec += ms*1000;
    tv.tv_sec += (tv.tv_usec / 1000000);
    tv.tv_usec %= 1000000;
}



#endif


