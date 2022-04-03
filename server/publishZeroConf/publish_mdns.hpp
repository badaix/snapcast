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

#ifndef PUBLISH_MDNS_HPP
#define PUBLISH_MDNS_HPP

// 3rd party headers
#include <boost/asio/io_context.hpp>

// standard headers
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
    PublishmDNS(const std::string& serviceName, boost::asio::io_context& ioc) : serviceName_(serviceName), io_context_(ioc)
    {
    }

    virtual ~PublishmDNS() = default;

    virtual void publish(const std::vector<mDNSService>& services) = 0;

protected:
    std::string serviceName_;
    boost::asio::io_context& io_context_;
};

#if defined(HAS_AVAHI)
#include "publish_avahi.hpp"
using PublishZeroConf = PublishAvahi;
#elif defined(HAS_BONJOUR)
#include "publish_bonjour.hpp"
using PublishZeroConf = PublishBonjour;
#endif

#endif
