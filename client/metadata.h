/***
    This file is part of snapcast
    Copyright (C) 2014-2017  Johannes Pohl

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

#ifndef METADATA_H
#define METADATA_H

//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <limits.h>
//#include <cstdlib>
//#include <cstring>
//#include <iostream>
//#include <streambuf>
#include "json.hpp"

// Prefix used in output
#define METADATA	std::string("metadata")

/*
 * Implement a generic metadata output handler
 */
using json = nlohmann::json;

/*
 * Base class, prints to stdout
 */
class MetadataAdapter
{
public:
	MetadataAdapter()
	{
		reset();
	}

        void reset()
	{
		_msg.reset(new json);
	}

	std::string serialize()
	{
		return METADATA + ":" + _msg->dump() + "\n";
	}

	void tag(std::string name, std::string value)
	{
		(*_msg)[name] = value;
	}

	std::string operator[](std::string key)
	{
		try {
			return (*_msg)[key];
		}
            	catch (std::domain_error&)
            	{
			return std::string();
            	}
	}

	virtual int push()
	{
		std::cout << serialize() << "\n";
		return 0;
	}

	int push(json& jtag)
	{
		_msg.reset(new json(jtag));
		return push();
	}

protected:
        std::shared_ptr<json> _msg;
};

/*
 * Send metadata to stderr as json
 */
class MetaStderrAdapter: public MetadataAdapter
{
public:
	using MetadataAdapter::push;

	int push()
	{
		std::cerr << serialize() << "\n";
		return 0;
	}

};

#endif
