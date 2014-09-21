#ifndef TIME_PROVIDER_H
#define TIME_PROVIDER_H

#include <atomic>
#include "doubleBuffer.h"

class TimeProvider
{
public:
	static TimeProvider& getInstance()
	{
		static TimeProvider instance;
		return instance;
	}

	void setDiffToServer(double ms);
	long getDiffToServer();
	long getPercentileDiffToServer(size_t percentile);
	long getDiffToServerMs();

private:
	TimeProvider(); 
	TimeProvider(TimeProvider const&);   // Don't Implement
	void operator=(TimeProvider const&); // Don't implement

	DoubleBuffer<long> diffBuffer;
	std::atomic<long> diffToServer;
};


#endif


