/***
  This file is part of avahi.

  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#include <cstdio>
#include <cstdlib>
#include "publishAvahi.h"
#include "common/log.h"


static AvahiEntryGroup *group;
static AvahiSimplePoll *simple_poll;
static char* name;

PublishAvahi::PublishAvahi(const std::string& serviceName) : PublishmDNS(serviceName),
	client_(NULL), active_(false)
{
	group = NULL;
	simple_poll = NULL;
	name = avahi_strdup(serviceName_.c_str());
}


void PublishAvahi::publish(const std::vector<mDNSService>& services)
{
	services_ = services;

	/// Allocate main loop object
	if (!(simple_poll = avahi_simple_poll_new()))
	{
		///TODO: error handling
		logE << "Failed to create simple poll object.\n";
	}

	/// Allocate a new client
	int error;
	client_ = avahi_client_new(avahi_simple_poll_get(simple_poll), AVAHI_CLIENT_IGNORE_USER_CONFIG, client_callback, this, &error);

	/// Check wether creating the client object succeeded
	if (!client_)
	{
		logE << "Failed to create client: " << avahi_strerror(error) << "\n";
	}

	active_ = true;
	pollThread_ = std::thread(&PublishAvahi::worker, this);
}


void PublishAvahi::worker()
{
	while (active_ && (avahi_simple_poll_iterate(simple_poll, 100) == 0));
}


PublishAvahi::~PublishAvahi()
{
	active_ = false;
	pollThread_.join();

	if (client_)
		avahi_client_free(client_);

	if (simple_poll)
		avahi_simple_poll_free(simple_poll);

	avahi_free(name);
}


void PublishAvahi::entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata)
{
	assert(g == group || group == NULL);
	group = g;

	/// Called whenever the entry group state changes
	switch (state)
	{
		case AVAHI_ENTRY_GROUP_ESTABLISHED :
			/// The entry group has been established successfully
			logO << "Service '" << name << "' successfully established.\n";
			break;

		case AVAHI_ENTRY_GROUP_COLLISION : 
		{
			char *n;

			/// A service name collision with a remote service happened. Let's pick a new name
			n = avahi_alternative_service_name(name);
			avahi_free(name);
			name = n;

			logO << "Service name collision, renaming service to '" << name << "'\n";

			/// And recreate the services
			static_cast<PublishAvahi*>(userdata)->create_services(avahi_entry_group_get_client(g));
			break;
		}

		case AVAHI_ENTRY_GROUP_FAILURE :

			logE << "Entry group failure: " << avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))) << "\n";

			/// Some kind of failure happened while we were registering our services
			avahi_simple_poll_quit(simple_poll);
			break;

		case AVAHI_ENTRY_GROUP_UNCOMMITED:
		case AVAHI_ENTRY_GROUP_REGISTERING:
			;
	}
}

void PublishAvahi::create_services(AvahiClient *c)
{
	assert(c);
	char *n;
	
	/// If this is the first time we're called, let's create a new entry group if necessary
	if (!group)
	{
		if (!(group = avahi_entry_group_new(c, entry_group_callback, this)))
		{
			logE << "avahi_entry_group_new() failed: " << avahi_strerror(avahi_client_errno(c)) << "\n";
			goto fail;
		}
	}

	/// If the group is empty (either because it was just created, or because it was reset previously, add our entries. 
	int ret;
	if (avahi_entry_group_is_empty(group))
	{
		logO << "Adding service '" << name << "'\n";

		/// We will now add two services and one subtype to the entry group
		for (const auto& service: services_)
		{
			if ((ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AvahiPublishFlags(0), name, service.name_.c_str(), NULL, NULL, service.port_, NULL)) < 0)
			{
				if (ret == AVAHI_ERR_COLLISION)
					goto collision;

				logE << "Failed to add " << service.name_ << " service: " << avahi_strerror(ret) << "\n";
				goto fail;
			}
		}

		/// Add an additional (hypothetic) subtype
/*		if ((ret = avahi_entry_group_add_service_subtype(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, AvahiPublishFlags(0), name, "_printer._tcp", NULL, "_magic._sub._printer._tcp") < 0))
		{
			fprintf(stderr, "Failed to add subtype _magic._sub._printer._tcp: %s\n", avahi_strerror(ret));
			goto fail;
		}
*/
		/// Tell the server to register the service
		if ((ret = avahi_entry_group_commit(group)) < 0)
		{
			logE << "Failed to commit entry group: " << avahi_strerror(ret) << "\n";
			goto fail;
		}
	}

	return;

collision:

	/// A service name collision with a local service happened. Let's pick a new name
	n = avahi_alternative_service_name(name);
	avahi_free(name);
	name = n;

	logO << "Service name collision, renaming service to '" << name << "'\n";

	avahi_entry_group_reset(group);

	create_services(c);
	return;

fail:
	avahi_simple_poll_quit(simple_poll);
}


void PublishAvahi::client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata)
{
	assert(c);

	/// Called whenever the client or server state changes
	switch (state)
	{
		case AVAHI_CLIENT_S_RUNNING:

			/// The server has startup successfully and registered its host name on the network, so it's time to create our services
			static_cast<PublishAvahi*>(userdata)->create_services(c);
			break;

		case AVAHI_CLIENT_FAILURE:

			logE << "Client failure: " << avahi_strerror(avahi_client_errno(c)) << "\n";
			avahi_simple_poll_quit(simple_poll);
			break;

		case AVAHI_CLIENT_S_COLLISION:

			/// Let's drop our registered services. When the server is back
			/// in AVAHI_SERVER_RUNNING state we will register them again with the new host name.

		case AVAHI_CLIENT_S_REGISTERING:

			/// The server records are now being established. This might be caused by a host name change. We need to wait
			/// for our own records to register until the host name is properly esatblished.

			if (group)
				avahi_entry_group_reset(group);
			break;

		case AVAHI_CLIENT_CONNECTING:
			;
	}
}



