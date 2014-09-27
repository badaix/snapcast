#ifndef TIME_DEFS_H
#define TIME_DEFS_H

#include <chrono>

namespace chronos
{
	template <class Rep, class Period>
	std::chrono::duration<Rep, Period> abs(std::chrono::duration<Rep, Period> d)
	{
		Rep x = d.count(); 
		return std::chrono::duration<Rep, Period>(x >= 0 ? x : -x);
	}

	typedef std::chrono::high_resolution_clock hrc;
	typedef std::chrono::time_point<hrc> time_point_hrc;
	typedef std::chrono::seconds sec;
	typedef std::chrono::milliseconds msec;
	typedef std::chrono::microseconds usec;
	typedef std::chrono::nanoseconds nsec;
}


#endif


