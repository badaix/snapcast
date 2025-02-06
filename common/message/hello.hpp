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

// local headers
#include "common/str_compat.hpp"
#include "common/utils.hpp"
#include "json_message.hpp"

// standard headers
#include <optional>
#include <string>


namespace msg
{

/// Hello message
/// Initial message, sent from client to server
class Hello : public JsonMessage
{
public:
    /// c'tor
    Hello() : JsonMessage(message_type::kHello)
    {
    }

    /// c'tor taking @p macAddress, @p id and @p instance
    Hello(const std::string& mac_address, const std::string& id, size_t instance, std::optional<std::string> username, std::optional<std::string> password)
        : JsonMessage(message_type::kHello)
    {
        msg["MAC"] = mac_address;
        msg["HostName"] = ::getHostName();
        msg["Version"] = VERSION;
        msg["ClientName"] = "Snapclient";
        msg["OS"] = ::getOS();
        msg["Arch"] = ::getArch();
        msg["Instance"] = instance;
        msg["ID"] = id;
        if (username.has_value())
            msg["Username"] = username.value();
        if (password.has_value())
            msg["Password"] = password.value();
        msg["SnapStreamProtocolVersion"] = 2;
    }

    /// d'tor
    ~Hello() override = default;

    /// @return the MAC address
    std::string getMacAddress() const
    {
        return msg["MAC"];
    }

    /// @return the host name
    std::string getHostName() const
    {
        return msg["HostName"];
    }

    /// @return the client version
    std::string getVersion() const
    {
        return msg["Version"];
    }

    /// @return the client name (e.g. "Snapclient")
    std::string getClientName() const
    {
        return msg["ClientName"];
    }

    /// @return the OS name
    std::string getOS() const
    {
        return msg["OS"];
    }

    /// @return the CPU architecture
    std::string getArch() const
    {
        return msg["Arch"];
    }

    /// @return the instance id
    int getInstance() const
    {
        return get("Instance", 1);
    }

    /// @return the protocol version
    int getProtocolVersion() const
    {
        return get("SnapStreamProtocolVersion", 1);
    }

    /// @return a unqiue machine ID
    std::string getId() const
    {
        return get("ID", getMacAddress());
    }

    /// @return a unqiue client ID
    std::string getUniqueId() const
    {
        std::string id = getId();
        int instance = getInstance();
        if (instance != 1)
        {
            id = id + "#" + cpt::to_string(instance);
        }
        return id;
    }

    /// @return the username
    std::optional<std::string> getUsername() const
    {
        if (!msg.contains("Username"))
            return std::nullopt;
        return msg["Username"];
    }

    /// @return the password
    std::optional<std::string> getPassword() const
    {
        if (!msg.contains("Password"))
            return std::nullopt;
        return msg["Password"];
    }
};

} // namespace msg
