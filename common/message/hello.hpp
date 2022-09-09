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

#ifndef MESSAGE_HELLO_HPP
#define MESSAGE_HELLO_HPP

// local headers
#include "common/str_compat.hpp"
#include "common/utils.hpp"
#include "json_message.hpp"

// standard headers
#include <string>


namespace msg
{

class Hello : public JsonMessage
{
public:
    Hello() : JsonMessage(message_type::kHello)
    {
    }

    Hello(const std::string& macAddress, const std::string& id, size_t instance) : JsonMessage(message_type::kHello)
    {
        msg["MAC"] = macAddress;
        msg["HostName"] = ::getHostName();
        msg["Version"] = VERSION;
        msg["ClientName"] = "Snapclient";
        msg["OS"] = ::getOS();
        msg["Arch"] = ::getArch();
        msg["Instance"] = instance;
        msg["ID"] = id;
        msg["SnapStreamProtocolVersion"] = 2;
    }

    ~Hello() override = default;

    std::string getMacAddress() const
    {
        return msg["MAC"];
    }

    std::string getHostName() const
    {
        return msg["HostName"];
    }

    std::string getVersion() const
    {
        return msg["Version"];
    }

    std::string getClientName() const
    {
        return msg["ClientName"];
    }

    std::string getOS() const
    {
        return msg["OS"];
    }

    std::string getArch() const
    {
        return msg["Arch"];
    }

    int getInstance() const
    {
        return get("Instance", 1);
    }

    int getProtocolVersion() const
    {
        return get("SnapStreamProtocolVersion", 1);
    }

    std::string getId() const
    {
        return get("ID", getMacAddress());
    }

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
};
} // namespace msg


#endif
