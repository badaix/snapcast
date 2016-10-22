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

#include "browseAvahi.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include "common/snapException.h"
#include "common/log.h"


static AvahiSimplePoll *simple_poll = NULL;


BrowseAvahi::BrowseAvahi() : client_(NULL), sb_(NULL)
{
}


BrowseAvahi::~BrowseAvahi()
{
	cleanUp();
}


void BrowseAvahi::cleanUp()
{
	if (sb_)
		avahi_service_browser_free(sb_);
	sb_ = NULL;

	if (client_)
		avahi_client_free(client_);
	client_ = NULL;

	if (simple_poll)
		avahi_simple_poll_free(simple_poll);
	simple_poll = NULL;
}


void BrowseAvahi::resolve_callback(
	AvahiServiceResolver *r,
	AVAHI_GCC_UNUSED AvahiIfIndex interface,
	AVAHI_GCC_UNUSED AvahiProtocol protocol,
	AvahiResolverEvent event,
	const char *name,
	const char *type,
	const char *domain,
	const char *host_name,
	const AvahiAddress *address,
	uint16_t port,
	AvahiStringList *txt,
	AvahiLookupResultFlags flags,
	AVAHI_GCC_UNUSED void* userdata)
{
	BrowseAvahi* browseAvahi = static_cast<BrowseAvahi*>(userdata);
	assert(r);

	/* Called whenever a service has been resolved successfully or timed out */

	switch (event)
	{
		case AVAHI_RESOLVER_FAILURE:
			logE << "(Resolver) Failed to resolve service '" << name << "' of type '" << type << "' in domain '" << domain << "': " << avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))) << "\n";
			break;

		case AVAHI_RESOLVER_FOUND:
		{
			char a[AVAHI_ADDRESS_STR_MAX], *t;

			logO << "Service '" << name << "' of type '" << type << "' in domain '" << domain << "':\n";

			avahi_address_snprint(a, sizeof(a), address);
			browseAvahi->result_.host_ = host_name;
			browseAvahi->result_.ip_ = a;
			browseAvahi->result_.port_ = port;
			browseAvahi->result_.proto_ = protocol;
			browseAvahi->result_.valid_ = true;

			t = avahi_string_list_to_string(txt);
			logO << "\t" << host_name << ":" << port << " (" << a << ")\n";
			logD << "\tTXT=" << t << "\n";
			logD << "\tProto=" << (int)protocol << "\n";
			logD << "\tcookie is " << avahi_string_list_get_service_cookie(txt) << "\n";
			logD << "\tis_local: " << !!(flags & AVAHI_LOOKUP_RESULT_LOCAL) << "\n";
			logD << "\tour_own: " << !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN) << "\n";
			logD << "\twide_area: " << !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA) << "\n";
			logD << "\tmulticast: " << !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST) << "\n";
			logD << "\tcached: " << !!(flags & AVAHI_LOOKUP_RESULT_CACHED) << "\n";
			avahi_free(t);
		}
	}

	avahi_service_resolver_free(r);
}


void BrowseAvahi::browse_callback(
	AvahiServiceBrowser *b,
	AvahiIfIndex interface,
	AvahiProtocol protocol,
	AvahiBrowserEvent event,
	const char *name,
	const char *type,
	const char *domain,
	AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
	void* userdata)
{

//    AvahiClient* client = (AvahiClient*)userdata;
	BrowseAvahi* browseAvahi = static_cast<BrowseAvahi*>(userdata);
	assert(b);

	/* Called whenever a new services becomes available on the LAN or is removed from the LAN */

	switch (event)
	{
		case AVAHI_BROWSER_FAILURE:
			logE << "(Browser) " << avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))) << "\n";
			avahi_simple_poll_quit(simple_poll);
			return;

		case AVAHI_BROWSER_NEW:
			logO << "(Browser) NEW: service '" << name << "' of type '" << type << "' in domain '" << domain << "'\n";

			/* We ignore the returned resolver object. In the callback
			   function we free it. If the server is terminated before
			   the callback function is called the server will free
			   the resolver for us. */

			if (!(avahi_service_resolver_new(browseAvahi->client_, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)0, resolve_callback, userdata)))
				logE << "Failed to resolve service '" << name << "': " << avahi_strerror(avahi_client_errno(browseAvahi->client_)) << "\n";

			break;

		case AVAHI_BROWSER_REMOVE:
			logO << "(Browser) REMOVE: service '" << name << "' of type '" << type << "' in domain '" << domain << "'\n";
			break;

		case AVAHI_BROWSER_ALL_FOR_NOW:
		case AVAHI_BROWSER_CACHE_EXHAUSTED:
			logO << "(Browser) " << (event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW") << "\n";
			break;
	}
}


void BrowseAvahi::client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata)
{
	assert(c);

	/* Called whenever the client or server state changes */
//    BrowseAvahi* browseAvahi = static_cast<BrowseAvahi*>(userdata);

	if (state == AVAHI_CLIENT_FAILURE)
	{
		logE << "Server connection failure: " << avahi_strerror(avahi_client_errno(c)) << "\n";
		avahi_simple_poll_quit(simple_poll);
	}
}


bool BrowseAvahi::browse(const std::string& serviceName, mDNSResult& result, int timeout)
{
	try
	{
		/* Allocate main loop object */
		if (!(simple_poll = avahi_simple_poll_new()))
			throw SnapException("BrowseAvahi - Failed to create simple poll object");

		/* Allocate a new client */
		int error;
		if (!(client_ = avahi_client_new(avahi_simple_poll_get(simple_poll), (AvahiClientFlags)0, client_callback, this, &error)))
			throw SnapException("BrowseAvahi - Failed to create client: " + std::string(avahi_strerror(error)));

		/* Create the service browser */
		if (!(sb_ = avahi_service_browser_new(client_, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, serviceName.c_str(), NULL, (AvahiLookupFlags)0, browse_callback, this)))
			throw SnapException("BrowseAvahi - Failed to create service browser: " + std::string(avahi_strerror(avahi_client_errno(client_))));

		result_.valid_ = false;
		while (timeout > 0)
		{
			avahi_simple_poll_iterate(simple_poll, 100);
			timeout -= 100;
			if (result_.valid_)
			{
				result = result_;
				cleanUp();
				return true;
			}
		}

		cleanUp();
		return false;
	}
	catch (...)
	{
		cleanUp();
		throw;
	}
}


