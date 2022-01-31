/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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


#ifndef PUBLISH_AVAHI_HPP
#define PUBLISH_AVAHI_HPP

// local headers

// 3rd party headers
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/timeval.h>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <atomic>
#include <string>
#include <vector>

class PublishAvahi;

#include "publish_mdns.hpp"

class PublishAvahi : public PublishmDNS
{
public:
    PublishAvahi(const std::string& serviceName, boost::asio::io_context& ioc);
    ~PublishAvahi() override;
    void publish(const std::vector<mDNSService>& services) override;

private:
    static void entry_group_callback(AvahiEntryGroup* g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void* userdata);
    static void client_callback(AvahiClient* c, AvahiClientState state, AVAHI_GCC_UNUSED void* userdata);
    void create_services(AvahiClient* c);
    void poll();
    AvahiClient* client_;
    std::vector<mDNSService> services_;
    boost::asio::steady_timer timer_;
};


#endif
