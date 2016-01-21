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

#include "mapMessage.h"
#include "common/utils.h"
#include <string>


namespace msg
{

class Hello : public MapMessage
{
public:
	Hello() : MapMessage(message_type::kHello)
	{
	}

	Hello(const std::string& macAddress) : MapMessage(message_type::kHello)
	{
		add("MAC", macAddress);
		add("HostName", ::getHostName());
		add("Version", VERSION);
	}

	virtual ~Hello()
	{
	}

	std::string getMacAddress()
	{
		return get("MAC");
	}

	std::string getHostName()
	{
		return get("HostName");
	}

	std::string getVersion()
	{
		return get("Version");
	}

};

}


#endif


