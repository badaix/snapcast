#include "timeProvider.h"


TimeProvider::TimeProvider() : diffToServer(0)
{
	diffBuffer.setSize(100);
}


void TimeProvider::setDiffToServer(double ms)
{
	diffBuffer.add(ms * 1000);
	diffToServer = diffBuffer.median();
}

/*
long TimeProvider::getPercentileDiffToServer(size_t percentile)
{
	return diffBuffer.percentile(percentile);
}
*/

