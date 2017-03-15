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

#ifndef JSON_MESSAGE_H
#define JSON_MESSAGE_H

#include "message.h"

#ifdef HAS_JSONRPCPP
#include <jsonrpcpp/json.hpp>
#else
#include "externals/json.hpp"
#endif


using json = nlohmann::json;


namespace msg
{

class JsonMessage : public BaseMessage
{
public:
	JsonMessage(message_type msgType) : BaseMessage(msgType)
	{
	}

	virtual ~JsonMessage()
	{
	}

	virtual void read(std::istream& stream)
	{
		std::string s;
		readVal(stream, s);
		msg = json::parse(s);
	}

	virtual uint32_t getSize() const
	{
		return sizeof(uint32_t) + msg.dump().size();
	}

	json msg;


protected:
	virtual void doserialize(std::ostream& stream) const
	{
		writeVal(stream, msg.dump());
	}

	template<typename T>
	T get(const std::string& what, const T& def) const
	{
		try
		{
			if (!msg.count(what))
				return def;
			return msg[what].get<T>();
		}
		catch(...)
		{
			return def;
		}
	}
};

}


#endif


