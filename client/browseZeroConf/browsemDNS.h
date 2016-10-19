#ifndef BROWSEMDNS_H
#define BROWSEMDNS_H

#include <string>

struct mDNSResult
{
	int proto_;
	std::string ip_;
	std::string host_;
	uint16_t port_;
	bool valid_;
};

class BrowsemDNS
{
public:
	virtual bool browse(const std::string& serviceName, mDNSResult& result, int timeout) = 0;
};

#if defined(HAS_AVAHI)
#include "browseAvahi.h"
typedef BrowseAvahi BrowseZeroConf;
#elif defined(HAS_BONJOUR)
#include "browseBonjour.h"
typedef BrowseBonjour BrowseZeroConf;
#endif

#endif
