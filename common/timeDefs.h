#ifndef TIME_DEFS_H
#define TIME_DEFS_H

#include <chrono>
#include <sys/time.h>

namespace chronos
{
	typedef std::chrono::high_resolution_clock hrc;
	typedef std::chrono::time_point<hrc> time_point_hrc;
	typedef std::chrono::seconds sec;
	typedef std::chrono::milliseconds msec;
	typedef std::chrono::microseconds usec;
	typedef std::chrono::nanoseconds nsec;

	static void addUs(timeval& tv, int us)
	{
		if (us < 0)
		{
			timeval t;
			t.tv_sec = -us / 1000000;
			t.tv_usec = (-us % 1000000);
			timersub(&tv, &t, &tv);
			return;
		}
		tv.tv_usec += us;
		tv.tv_sec += (tv.tv_usec / 1000000);
		tv.tv_usec %= 1000000;
	}

	static long getTickCount()
	{
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		return now.tv_sec*1000 + now.tv_nsec / 1000000;
	}

	template <class Rep, class Period>
	std::chrono::duration<Rep, Period> abs(std::chrono::duration<Rep, Period> d)
	{
		Rep x = d.count(); 
		return std::chrono::duration<Rep, Period>(x >= 0 ? x : -x);
	}

	template <class ToDuration, class Rep, class Period>
	int64_t duration(std::chrono::duration<Rep, Period> d)
	{
		return std::chrono::duration_cast<ToDuration>(d).count();
	}
}


#endif


