/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/


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

private:
	static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata);
	static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata);
	void create_services(AvahiClient *c);
    AvahiClient* client;
	std::string serviceName_;
	std::thread pollThread_;
	void worker();
	std::atomic<bool> active_;
	std::vector<AvahiService> services;
};


#endif


