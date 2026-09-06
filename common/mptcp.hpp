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

#ifdef HAS_MPTCP

// 3rd party headers
#include <boost/asio/basic_socket_acceptor.hpp>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/tcp.hpp>

// standard headers
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

// IPPROTO_MPTCP is defined in <linux/in.h>, which is not included here to
// avoid conflicts between Linux and libc headers. Some libc versions don't
// define it in <netinet/in.h> either, so define it if necessary.
#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif


namespace snapcast
{
namespace net
{

/// Boost.Asio protocol type for Multipath TCP (MPTCP)
/**
 * Behaves like boost::asio::ip::tcp, but creates sockets with protocol IPPROTO_MPTCP.
 * The connection falls back to regular TCP automatically if the peer doesn't support MPTCP.
 * Only available on Linux with MPTCP support in the kernel (defined via HAS_MPTCP).
 */
class mptcp
{
public:
    /// The type of an MPTCP endpoint
    using endpoint = boost::asio::ip::basic_endpoint<mptcp>;
    /// The type of a resolver that queries MPTCP endpoints
    using resolver = boost::asio::ip::basic_resolver<mptcp>;
    /// The type of an MPTCP socket
    using socket = boost::asio::basic_stream_socket<mptcp>;
    /// The type of an MPTCP acceptor
    using acceptor = boost::asio::basic_socket_acceptor<mptcp>;

    /// Construct to represent the IPv4 MPTCP protocol
    constexpr mptcp() noexcept : family_(AF_INET)
    {
    }

    /// Construct to represent the IPv6 MPTCP protocol
    constexpr explicit mptcp(int family) noexcept : family_(family)
    {
    }

    /// Obtain an identifier for the type of the protocol
    static constexpr int type() noexcept
    {
        return SOCK_STREAM;
    }

    /// Obtain an identifier for the protocol
    static constexpr int protocol() noexcept
    {
        return IPPROTO_MPTCP;
    }

    /// Obtain an identifier for the protocol family
    int family() const noexcept
    {
        return family_;
    }

    /// The MPTCP over IPv4 protocol type
    static constexpr mptcp v4() noexcept
    {
        return mptcp(AF_INET);
    }

    /// The MPTCP over IPv6 protocol type
    static constexpr mptcp v6() noexcept
    {
        return mptcp(AF_INET6);
    }

    /// Compare two protocols for equality
    friend bool operator==(const mptcp& lhs, const mptcp& rhs) noexcept
    {
        return lhs.family_ == rhs.family_;
    }

    /// Compare two protocols for inequality
    friend bool operator!=(const mptcp& lhs, const mptcp& rhs) noexcept
    {
        return !(lhs == rhs);
    }

private:
    int family_;
};


/// Convert a TCP endpoint to an MPTCP endpoint
inline mptcp::endpoint make_endpoint(const boost::asio::ip::tcp::endpoint& endpoint)
{
    return mptcp::endpoint(endpoint.address(), endpoint.port());
}


/// @param err if not nullptr, receives the errno of the failed availability check
/// @return true if the kernel allows creating MPTCP sockets
inline bool is_available(int* err = nullptr) noexcept
{
    static const int error = []()
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
        if (fd >= 0)
        {
            ::close(fd);
            return 0;
        }
        return errno;
    }();
    if (err != nullptr)
        *err = error;
    return error == 0;
}

} // namespace net
} // namespace snapcast

#endif // HAS_MPTCP
