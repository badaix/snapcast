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

#ifndef JSON_REQUEST_ID_H
#define JSON_REQUEST_ID_H

#include <string>
#include <stdexcept>
#include "externals/json.hpp"


using Json = nlohmann::json;


struct req_id
{
	enum class value_t : uint8_t
	{
		null = 0,
		string,
		integer
	};

	req_id() : type(value_t::null), int_id(0), string_id("")
	{
	}

	req_id(int id) : type(value_t::integer), int_id(id), string_id("")
	{
	}

	req_id(const std::string& id) : type(value_t::string), int_id(0), string_id(id)
	{
	}

	req_id(const Json& json_id) : type(value_t::null)
	{
		if (json_id.is_null())
		{
			type = value_t::null;
		}
		else if (json_id.is_number_integer())
		{
			int_id = json_id.get<int>();
			type = value_t::integer;
		}
		else if (json_id.is_string())
		{
			string_id = json_id.get<std::string>();
			type = value_t::string;
		}
		else
			throw std::invalid_argument("id must be integer, string or null");
	}

	Json toJson() const
	{
		if (type == value_t::string)
			return string_id;
		else if (type == value_t::integer)
			return int_id;
		else
			return nullptr;
	}

	friend std::ostream& operator<< (std::ostream &out, const req_id &id)
	{
		out << id.toJson();
		return out;
	}

	value_t type;
	int int_id;
	std::string string_id;
};


#endif

