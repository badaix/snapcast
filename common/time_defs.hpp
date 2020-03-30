/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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
#else // from the GNU C library implementation of sys/time.h

#include <winsock2.h>

#define timersub(a, b, result)                                                                                                                                 \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                                                                                                          \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                                                                                                       \
        if ((result)->tv_usec < 0)                                                                                                                             \
        {                                                                                                                                                      \
            --(result)->tv_sec;                                                                                                                                \
            (result)->tv_usec += 1000000;                                                                                                                      \
        }                                                                                                                                                      \
    } while (0)

#define CLOCK_MONOTONIC 42 // discarded on windows plaforms

// from http://stackoverflow.com/a/38212960/2510022
#define BILLION (1E9)

static BOOL g_first_time = 1;
static LARGE_INTEGER g_counts_per_sec;

inline static int clock_gettime(int dummy, struct timespec* ct)
{
    LARGE_INTEGER count;

    if (g_first_time)
    {
        g_first_time = 0;

        if (0 == QueryPerformanceFrequency(&g_counts_per_sec))
        {
            g_counts_per_sec.QuadPart = 0;
        }
    }

    if ((NULL == ct) || (g_counts_per_sec.QuadPart <= 0) || (0 == QueryPerformanceCounter(&count)))
    {
        return -1;
    }

    ct->tv_sec = count.QuadPart / g_counts_per_sec.QuadPart;
    ct->tv_nsec = ((count.QuadPart % g_counts_per_sec.QuadPart) * BILLION) / g_counts_per_sec.QuadPart;

    return 0;
}
#endif

namespace chronos
{
using clk = 
#ifndef WINDOWS
    std::chrono::steady_clock;
#else
    std::chrono::system_clock;
#endif
using time_point_clk = std::chrono::time_point<clk>;
using sec = std::chrono::seconds;
using msec = std::chrono::milliseconds;
using usec = std::chrono::microseconds;
using nsec = std::chrono::nanoseconds;

template <class Clock>
inline static void timeofday(struct timeval* tv)
{
    auto now = Clock::now();
    auto microsecs = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    tv->tv_sec = microsecs.count() / 1000000;
    tv->tv_usec = microsecs.count() % 1000000;
}

inline static void steadytimeofday(struct timeval* tv)
{
    timeofday<clk>(tv);
}

inline static void systemtimeofday(struct timeval* tv)
{
    timeofday<std::chrono::system_clock>(tv);
}

template <class ToDuration>
inline ToDuration diff(const timeval& tv1, const timeval& tv2)
{
    auto sec = tv1.tv_sec - tv2.tv_sec;
    auto usec = tv1.tv_usec - tv2.tv_usec;
    while (usec < 0)
    {
        sec -= 1;
        usec += 1000000;
    }
    return std::chrono::duration_cast<ToDuration>(std::chrono::seconds(sec) + std::chrono::microseconds(usec));
}


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
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    return mts.tv_sec * 1000 + mts.tv_nsec / 1000000;
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000 + now.tv_nsec / 1000000;
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
template <class Rep, class Period>
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
} // namespace chronos


#endif
