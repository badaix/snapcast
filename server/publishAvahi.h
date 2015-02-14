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

class PublishAvahi
{
public:
	PublishAvahi();
	~PublishAvahi();
		
private:
	void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata);
	void create_services(AvahiClient *c);
	void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata);

	AvahiEntryGroup *group;
	AvahiSimplePoll *simple_poll;
	char* name;
};


#endif


