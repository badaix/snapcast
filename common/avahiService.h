#ifndef AVAHI_SERVICE_H
#define AVAHI_SERVICE_H

#include <avahi-common/address.h>
#include <string>

struct AvahiService
{
	AvahiService(const std::string& name, size_t port, int proto = AVAHI_PROTO_UNSPEC) : name_(name), port_(port), proto_(proto)
	{
	}

	std::string name_;
	size_t port_;
	int proto_;
};


#endif


