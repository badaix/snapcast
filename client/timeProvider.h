/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#ifndef TIME_PROVIDER_H
#define TIME_PROVIDER_H

#include <atomic>
#include <chrono>
#include "doubleBuffer.h"
#include "message/message.h"
#include "common/timeDefs.h"


class TimeProvider
{
public:
	static TimeProvider& getInstance()
	{
		static TimeProvider instance;
		return instance;
	}

	void setDiffToServer(double ms);

	template<typename T>
	inline T getDiffToServer() const
	{
		return std::chrono::duration_cast<T>(chronos::usec(diffToServer_));
	}

/*	chronos::usec::rep getDiffToServer();
	chronos::usec::rep getPercentileDiffToServer(size_t percentile);
	long getDiffToServerMs();
*/

	template<typename T>
	static T sinceEpoche(const chronos::time_point_hrc& point) 
	{
		return std::chrono::duration_cast<T>(point.time_since_epoch());
	}

	static chronos::time_point_hrc toTimePoint(const tv& timeval)
	{
		return chronos::time_point_hrc(chronos::usec(timeval.usec) + chronos::sec(timeval.sec));
	}

	inline static chronos::time_point_hrc now()
	{
		return chronos::hrc::now();
	}

	inline static chronos::time_point_hrc serverNow()
	{
		return chronos::hrc::now() + TimeProvider::getInstance().getDiffToServer<chronos::usec>();
	}

private:
	TimeProvider(); 
	TimeProvider(TimeProvider const&);   // Don't Implement
	void operator=(TimeProvider const&); // Don't implement

	DoubleBuffer<chronos::usec::rep> diffBuffer_;
	std::atomic<chronos::usec::rep> diffToServer_;
};


#endif


