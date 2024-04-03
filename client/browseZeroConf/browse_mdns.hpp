/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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

// standard headers
#include <cstdint>
#include <string>

enum IPVersion
{
    IPv4 = 0,
    IPv6 = 1
};


struct mDNSResult
{
    mDNSResult() : ip_version(IPv4), iface_idx(0), port(0), valid(false)
    {
    }

    IPVersion ip_version;
    int iface_idx;
    std::string ip;
    std::string host;
    uint16_t port;
    bool valid;
};

class BrowsemDNS
{
public:
    virtual bool browse(const std::string& serviceName, mDNSResult& result, int timeout) = 0;
};

#if defined(HAS_AVAHI)
#include "browse_avahi.hpp"
using BrowseZeroConf = BrowseAvahi;
#elif defined(HAS_BONJOUR)
#include "browse_bonjour.hpp"
using BrowseZeroConf = BrowseBonjour;
#endif
