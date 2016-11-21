/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#ifndef HELLO_MSG_H
#define HELLO_MSG_H

#include "jsonMessage.h"
#include "common/utils.h"
#include <string>


namespace msg
{

class Hello : public JsonMessage
{
public:
	Hello() : JsonMessage(message_type::kHello)
	{
	}

	Hello(const std::string& macAddress, size_t instance) : JsonMessage(message_type::kHello)
	{
		msg["MAC"] = macAddress;
		msg["HostName"] = ::getHostName();
		msg["Version"] = VERSION;
		msg["ClientName"] = "Snapclient";
		msg["OS"] = ::getOS();
		msg["Arch"] = ::getArch();
		msg["Instance"] = instance;
		msg["SnapStreamProtocolVersion"] = 2;
	}

	virtual ~Hello()
	{
	}

	std::string getMacAddress()
	{
		return msg["MAC"];
	}

	std::string getHostName()
	{
		return msg["HostName"];
	}

	std::string getVersion()
	{
		return msg["Version"];
	}

	std::string getClientName()
	{
		return msg["ClientName"];
	}

	std::string getOS()
	{
		return msg["OS"];
	}

	std::string getArch()
	{
		return msg["Arch"];
	}

	int getInstance()
	{
		return get("Instance", 1);
	}

	int getProtocolVersion()
	{
		return get("SnapStreamProtocolVersion", 1);
	}

};

}


#endif


