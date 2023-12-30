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
    mDNSResult() : ip_version(IPv4), iface_idx(0), port(0), valid(false)
    {
    }

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
#include "browse_avahi.hpp"
using BrowseZeroConf = BrowseAvahi;
#elif defined(HAS_BONJOUR)
#include "browse_bonjour.hpp"
using BrowseZeroConf = BrowseBonjour;
#endif

#endif
