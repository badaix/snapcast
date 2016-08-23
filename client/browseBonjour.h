#ifndef BROWSEBONJOUR_H
#define BROWSEBONJOUR_H

#include <dns_sd.h>

class BrowseBonjour;

#include "browsemDNS.h"

class BrowseBonjour : public BrowsemDNS
{
public:
	bool browse(const std::string& serviceName, mDNSResult& result, int timeout) override;
};
#endif
