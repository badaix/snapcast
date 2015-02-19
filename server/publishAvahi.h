#ifndef PUBLISH_AVAHI_H
#define PUBLISH_AVAHI_H

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>


struct AvahiService
{
	AvahiService(const std::string& name, size_t port, int proto = AVAHI_PROTO_UNSPEC) : name_(name), port_(port), proto_(proto)
	{
	}

	std::string name_;
	size_t port_;
	int proto_;
};


class PublishAvahi
{
public:
	PublishAvahi(const std::string& serviceName);
	~PublishAvahi();
	void publish(const std::vector<AvahiService>& services);
	std::vector<AvahiService> services;

private:
	static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata);
	static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata);
	void create_services(AvahiClient *c);
    AvahiClient* client;
	std::string serviceName_;
	std::thread pollThread_;
	void worker();
	std::atomic<bool> active_;
};


#endif


