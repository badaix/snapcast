#ifndef TIME_DEFS_H
#define TIME_DEFS_H

#include <chrono>

namespace chronos
{
	typedef std::chrono::high_resolution_clock hrc;
	typedef std::chrono::time_point<hrc> time_point_hrc;
	typedef std::chrono::seconds sec;
	typedef std::chrono::milliseconds msec;
	typedef std::chrono::microseconds usec;
}


#endif


