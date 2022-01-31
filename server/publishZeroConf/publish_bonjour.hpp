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


#ifndef PUBLISH_BONJOUR_HPP
#define PUBLISH_BONJOUR_HPP

// 3rd party headers
#include <dns_sd.h>

// standard headers
#include <string>
#include <thread>

class PublishBonjour;

#include "publish_mdns.hpp"

class PublishBonjour : public PublishmDNS
{
public:
    PublishBonjour(const std::string& serviceName, boost::asio::io_context& ioc);
    virtual ~PublishBonjour();
    virtual void publish(const std::vector<mDNSService>& services);

private:
    std::thread pollThread_;
    void worker();
    std::atomic<bool> active_;
    std::vector<DNSServiceRef> clients;
};


#endif
