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
#ifdef MACOS
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#ifndef WINDOWS

#include <sys/time.h>

#else

#include <WinSock2.h>
// from the GNU C library implementation of sys/time.h
# define timersub(a, b, result)                                               \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0)

#endif

namespace chronos
{
	typedef std::chrono::system_clock clk;
	typedef std::chrono::time_point<clk> time_point_clk;

	#ifdef WINDOWS
	// Epoch is January 1st 1601
	// Period is 100ns
	class filetime_clock
	{
		typedef std::chrono::duration<uint64, std::ratio<1, 10000000> > duration;
		typedef duration::rep rep;
		typedef duration::period period;
		typedef std::chrono::time_point<filetime_clock> time_point;
		static const bool is_steady = false;

		static time_point now() noexcept
		{
			FILETIME time;
			GetSystemTimePreciseAsFileTime(&time); // oh so eloquently named
			return time_point(duration((time.dwHighDateTime << 32) + time.dwLowDateTime));
		}
	};
	#endif

	template<typename duration>
	void to_timeval(duration&& d, timeval& tv)
	{
		const auto sec = std::chrono::duration_cast<std::chrono::seconds>(d);
		
		tv.tv_sec = sec.count();
		tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(d - sec).count();
	}

	#ifndef WINDOWS
	typedef std::chrono::system_clock clk;
	#else
	typedef filetime_clock clk;
	#endif
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
		return std::chrono::duration_cast<std::chrono::milliseconds>(clk::now().time_since_epoch()).count();
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


