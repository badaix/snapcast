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

// standard headers
#include <cstdint>
#include <string>

/// IP versions IPv4 and IPv6
enum class IPVersion
{
    IPv4 = 0, ///< IPv4
    IPv6 = 1  ///< IPv6
};

/// mDNS record
struct mDNSResult
{
    IPVersion ip_version{IPVersion::IPv4}; ///< IP version
    int iface_idx{0};                      ///< interface index
    std::string ip;                        ///< IP address
    std::string host;                      ///< host
    uint16_t port{0};                      ///< port
    bool valid{false};                     ///< valid
};

/// mDNS browser interface
class BrowsemDNS
{
public:
    /// get mDNS record for @p serviceName
    virtual bool browse(const std::string& serviceName, mDNSResult& result, int timeout) = 0;
};
