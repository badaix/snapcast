/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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

#include "common/json.hpp"

// Prefix used in output
#define METADATA std::string("metadata")

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

	virtual ~MetadataAdapter() = default;

	void reset()
	{
		msg_.reset(new json);
	}

	std::string serialize()
	{
		return METADATA + ":" + msg_->dump();
	}

	void tag(std::string name, std::string value)
	{
		(*msg_)[name] = value;
	}

	std::string operator[](std::string key)
	{
		try 
		{
			return (*msg_)[key];
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
		msg_.reset(new json(jtag));
		return push();
	}

protected:
	std::shared_ptr<json> msg_;
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
