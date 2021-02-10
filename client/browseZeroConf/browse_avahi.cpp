/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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

#include "browse_avahi.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>


static AvahiSimplePoll* simple_poll = nullptr;

static constexpr auto LOG_TAG = "Avahi";


BrowseAvahi::BrowseAvahi() : client_(nullptr), sb_(nullptr)
{
}


BrowseAvahi::~BrowseAvahi()
{
    cleanUp();
}


void BrowseAvahi::cleanUp()
{
    if (sb_ != nullptr)
        avahi_service_browser_free(sb_);
    sb_ = nullptr;

    if (client_ != nullptr)
        avahi_client_free(client_);
    client_ = nullptr;

    if (simple_poll != nullptr)
        avahi_simple_poll_free(simple_poll);
    simple_poll = nullptr;
}


void BrowseAvahi::resolve_callback(AvahiServiceResolver* r, AVAHI_GCC_UNUSED AvahiIfIndex interface, AVAHI_GCC_UNUSED AvahiProtocol protocol,
                                   AvahiResolverEvent event, const char* name, const char* type, const char* domain, const char* host_name,
                                   const AvahiAddress* address, uint16_t port, AvahiStringList* txt, AvahiLookupResultFlags flags,
                                   AVAHI_GCC_UNUSED void* userdata)
{
    auto* browseAvahi = static_cast<BrowseAvahi*>(userdata);
    assert(r);

    /* Called whenever a service has been resolved successfully or timed out */

    switch (event)
    {
        case AVAHI_RESOLVER_FAILURE:
            LOG(ERROR, LOG_TAG) << "(Resolver) Failed to resolve service '" << name << "' of type '" << type << "' in domain '" << domain
                                << "': " << avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))) << "\n";
            break;

        case AVAHI_RESOLVER_FOUND:
        {
            char a[AVAHI_ADDRESS_STR_MAX], *t;

            LOG(INFO, LOG_TAG) << "Service '" << name << "' of type '" << type << "' in domain '" << domain << "':\n";

            avahi_address_snprint(a, sizeof(a), address);
            browseAvahi->result_.host = host_name;
            browseAvahi->result_.ip = a;
            browseAvahi->result_.port = port;
            // protocol seems to be unreliable (0 for IPv4 and for IPv6)
            browseAvahi->result_.ip_version = (browseAvahi->result_.ip.find(':') == std::string::npos) ? (IPVersion::IPv4) : (IPVersion::IPv6);
            browseAvahi->result_.valid = true;
            browseAvahi->result_.iface_idx = interface;

            t = avahi_string_list_to_string(txt);
            LOG(INFO, LOG_TAG) << "\t" << host_name << ":" << port << " (" << a << ")\n";
            LOG(DEBUG, LOG_TAG) << "\tTXT=" << t << "\n";
            LOG(DEBUG, LOG_TAG) << "\tProto=" << static_cast<int>(protocol) << "\n";
            LOG(DEBUG, LOG_TAG) << "\tcookie is " << avahi_string_list_get_service_cookie(txt) << "\n";
            LOG(DEBUG, LOG_TAG) << "\tis_local: " << !((flags & AVAHI_LOOKUP_RESULT_LOCAL) == 0) << "\n";
            LOG(DEBUG, LOG_TAG) << "\tour_own: " << !((flags & AVAHI_LOOKUP_RESULT_OUR_OWN) == 0) << "\n";
            LOG(DEBUG, LOG_TAG) << "\twide_area: " << !((flags & AVAHI_LOOKUP_RESULT_WIDE_AREA) == 0) << "\n";
            LOG(DEBUG, LOG_TAG) << "\tmulticast: " << !((flags & AVAHI_LOOKUP_RESULT_MULTICAST) == 0) << "\n";
            LOG(DEBUG, LOG_TAG) << "\tcached: " << !((flags & AVAHI_LOOKUP_RESULT_CACHED) == 0) << "\n";
            avahi_free(t);
        }
    }

    avahi_service_resolver_free(r);
}


void BrowseAvahi::browse_callback(AvahiServiceBrowser* b, AvahiIfIndex interface, AvahiProtocol protocol, AvahiBrowserEvent event, const char* name,
                                  const char* type, const char* domain, AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void* userdata)
{

    //    AvahiClient* client = (AvahiClient*)userdata;
    auto* browseAvahi = static_cast<BrowseAvahi*>(userdata);
    assert(b);

    /* Called whenever a new services becomes available on the LAN or is removed from the LAN */

    switch (event)
    {
        case AVAHI_BROWSER_FAILURE:
            LOG(ERROR, LOG_TAG) << "(Browser) " << avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))) << "\n";
            avahi_simple_poll_quit(simple_poll);
            return;

        case AVAHI_BROWSER_NEW:
            LOG(INFO, LOG_TAG) << "(Browser) NEW: service '" << name << "' of type '" << type << "' in domain '" << domain << "'\n";

            /* We ignore the returned resolver object. In the callback
               function we free it. If the server is terminated before
               the callback function is called the server will free
               the resolver for us. */

            if ((avahi_service_resolver_new(browseAvahi->client_, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, static_cast<AvahiLookupFlags>(0),
                                            resolve_callback, userdata)) == nullptr)
                LOG(ERROR, LOG_TAG) << "Failed to resolve service '" << name << "': " << avahi_strerror(avahi_client_errno(browseAvahi->client_)) << "\n";

            break;

        case AVAHI_BROWSER_REMOVE:
            LOG(INFO, LOG_TAG) << "(Browser) REMOVE: service '" << name << "' of type '" << type << "' in domain '" << domain << "'\n";
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            LOG(INFO, LOG_TAG) << "(Browser) " << (event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW") << "\n";
            break;
    }
}


void BrowseAvahi::client_callback(AvahiClient* c, AvahiClientState state, AVAHI_GCC_UNUSED void* userdata)
{
    assert(c);

    /* Called whenever the client or server state changes */
    //    BrowseAvahi* browseAvahi = static_cast<BrowseAvahi*>(userdata);

    if (state == AVAHI_CLIENT_FAILURE)
    {
        LOG(ERROR, LOG_TAG) << "Server connection failure: " << avahi_strerror(avahi_client_errno(c)) << "\n";
        avahi_simple_poll_quit(simple_poll);
    }
}


bool BrowseAvahi::browse(const std::string& serviceName, mDNSResult& result, int timeout)
{
    try
    {
        /* Allocate main loop object */
        if ((simple_poll = avahi_simple_poll_new()) == nullptr)
            throw SnapException("BrowseAvahi - Failed to create simple poll object");

        /* Allocate a new client */
        int error;
        if ((client_ = avahi_client_new(avahi_simple_poll_get(simple_poll), static_cast<AvahiClientFlags>(0), client_callback, this, &error)) == nullptr)
            throw SnapException("BrowseAvahi - Failed to create client: " + std::string(avahi_strerror(error)));

        /* Create the service browser */
        if ((sb_ = avahi_service_browser_new(client_, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, serviceName.c_str(), nullptr, static_cast<AvahiLookupFlags>(0),
                                             browse_callback, this)) == nullptr)
            throw SnapException("BrowseAvahi - Failed to create service browser: " + std::string(avahi_strerror(avahi_client_errno(client_))));

        result_.valid = false;
        while (timeout > 0)
        {
            avahi_simple_poll_iterate(simple_poll, 100);
            timeout -= 100;
            if (result_.valid)
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
