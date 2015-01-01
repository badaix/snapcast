#include "timeProvider.h"


TimeProvider::TimeProvider() : diffToServer_(0)
{
	diffBuffer_.setSize(200);
}


void TimeProvider::setDiffToServer(double ms)
{
	diffBuffer_.add(ms * 1000);
	diffToServer_ = diffBuffer_.median();
}

/*
long TimeProvider::getPercentileDiffToServer(size_t percentile)
{
	return diffBuffer.percentile(percentile);
}
*/

