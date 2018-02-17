#ifndef BROWSEMDNS_H
#define BROWSEMDNS_H

#include <string>

enum IPVersion
{
	IPv4 = 0,
	IPv6 = 1
};


struct mDNSResult
{
	IPVersion ip_version;
	int iface_idx;
	std::string ip;
	std::string host;
	uint16_t port;
	bool valid;
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
