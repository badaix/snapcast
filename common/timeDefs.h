#ifndef TIME_DEFS_H
#define TIME_DEFS_H

#include <chrono>

namespace chronos
{
	using hrc = std::chrono::high_resolution_clock;
	using time_point_hrc = std::chrono::time_point<hrc>;
	using sec = std::chrono::seconds;
	using msec = std::chrono::milliseconds;
	using usec = std::chrono::microseconds;
}


#endif


