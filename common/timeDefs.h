/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef TIME_DEFS_H
#define TIME_DEFS_H

#include <chrono>
#include <thread>
#include <sys/time.h>
#ifdef MACOS
#include <mach/clock.h>
#include <mach/mach.h>
#endif

namespace chronos
{
	typedef std::chrono::system_clock clk;
	typedef std::chrono::time_point<clk> time_point_clk;
	typedef std::chrono::seconds sec;
	typedef std::chrono::milliseconds msec;
	typedef std::chrono::microseconds usec;
	typedef std::chrono::nanoseconds nsec;

	inline static void addUs(timeval& tv, int us)
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

	inline static long getTickCount()
	{
#ifdef MACOS
		clock_serv_t cclock;
		mach_timespec_t mts;
		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mts);
		mach_port_deallocate(mach_task_self(), cclock);
		return mts.tv_sec*1000 + mts.tv_nsec / 1000000;
#else
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		return now.tv_sec*1000 + now.tv_nsec / 1000000;
#endif
	}

	template <class Rep, class Period>
	inline std::chrono::duration<Rep, Period> abs(std::chrono::duration<Rep, Period> d)
	{
		Rep x = d.count();
		return std::chrono::duration<Rep, Period>(x >= 0 ? x : -x);
	}

	template <class ToDuration, class Rep, class Period>
	inline int64_t duration(std::chrono::duration<Rep, Period> d)
	{
		return std::chrono::duration_cast<ToDuration>(d).count();
	}

	/// some sleep functions. Just for convenience.
	template< class Rep, class Period >
	inline void sleep(const std::chrono::duration<Rep, Period>& sleep_duration)
	{
		std::this_thread::sleep_for(sleep_duration);
	}

	inline void sleep(const int32_t& milliseconds)
	{
		if (milliseconds < 0)
			return;
		sleep(msec(milliseconds));
	}

	inline void usleep(const int32_t& microseconds)
	{
		if (microseconds < 0)
			return;
		sleep(usec(microseconds));
	}
}


#endif


