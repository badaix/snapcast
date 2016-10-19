#ifndef PUBLISH_MDNS_H
#define PUBLISH_MDNS_H

#include <string>
#include <vector>


struct mDNSService
{
	mDNSService(const std::string& name, size_t port) : name_(name), port_(port)
	{
	}

	std::string name_;
	size_t port_;
};


class PublishmDNS
{
public:
	PublishmDNS(const std::string& serviceName) : serviceName_(serviceName)
	{
	}

	virtual ~PublishmDNS()
	{
	}

	virtual void publish(const std::vector<mDNSService>& services) = 0;

protected:
	std::string serviceName_;
};

#if defined(HAS_AVAHI)
#include "publishAvahi.h"
typedef PublishAvahi PublishZeroConf;
#elif defined(HAS_BONJOUR)
#include "publishBonjour.h"
typedef PublishBonjour PublishZeroConf;
#endif

#endif
