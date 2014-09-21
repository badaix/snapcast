#include "timeProvider.h"


TimeProvider::TimeProvider() : diffToServer(0)
{
	diffBuffer.setSize(120);
}


void TimeProvider::setDiffToServer(double ms)
{
	diffBuffer.add(ms * 1000);
	diffToServer = diffBuffer.median();
}


long TimeProvider::getDiffToServer()
{
	return diffToServer;
}


long TimeProvider::getDiffToServerMs()
{
	return diffToServer / 1000;
}


long TimeProvider::getPercentileDiffToServer(size_t percentile)
{
	return diffBuffer.percentile(percentile);
}


