#ifndef BROWSEBONJOUR_H
#define BROWSEBONJOUR_H

#include <dns_sd.h>

class BrowseBonjour;

#include "browsemDNS.h"

class BrowseBonjour : public BrowsemDNS
{
public:
	BrowseBonjour();
	~BrowseBonjour();
	
	bool browse(const std::string& serviceName, mDNSResult& result, int timeout);
private:
	static void BonjourResolve(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType, const char*, const char*, uint16_t, uint16_t, const unsigned char*, void*);
  static void BonjourReply(DNSServiceRef, DNSServiceFlags, uint32_t, DNSServiceErrorType, const char*, const char*, const char*, void*);
};
#endif
