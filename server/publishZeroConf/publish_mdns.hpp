/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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

#pragma once


// 3rd party headers
#include <boost/asio/io_context.hpp>

// standard headers
#include <string>
#include <vector>


/// mDNS service
struct mDNSService
{
    /// c'tor
    mDNSService(std::string name, size_t port) : name_(std::move(name)), port_(port)
    {
    }

    std::string name_; ///< service name
    size_t port_;      ///< service port
};


/// mDNS publisher interface
class PublishmDNS
{
public:
    /// c'tor
    PublishmDNS(std::string serviceName, boost::asio::io_context& ioc) : serviceName_(std::move(serviceName)), io_context_(ioc)
    {
    }

    /// d'tor
    virtual ~PublishmDNS() = default;

    /// publish list of @p services
    virtual void publish(const std::vector<mDNSService>& services) = 0;

protected:
    std::string serviceName_;             ///< service name
    boost::asio::io_context& io_context_; ///< asio io context
};
