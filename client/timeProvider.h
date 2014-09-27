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
		return std::chrono::duration_cast<T>(chronos::usec(diffToServer));
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

	DoubleBuffer<chronos::usec::rep> diffBuffer;
	std::atomic<chronos::usec::rep> diffToServer;
};


#endif


