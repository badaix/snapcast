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

#ifndef MAP_MESSAGE_H
#define MAP_MESSAGE_H

#include "message.h"
#include <string>
#include <map>
#include <sstream>
#include "common/log.h"


namespace msg
{

class MapMessage : public BaseMessage
{
public:
	MapMessage(message_type msgType) : BaseMessage(msgType)
	{
	}

	virtual ~MapMessage()
	{
	}

	virtual void read(std::istream& stream)
	{
		strMap.clear();
		uint16_t mapSize;
		readVal(stream, mapSize);

		for (size_t n=0; n<mapSize; ++n)
		{
			std::string key;
			std::string value;

			readVal(stream, key);
			readVal(stream, value);
			strMap[key] = value;
		}
	}

	virtual uint32_t getSize() const
	{
		uint32_t size = sizeof(uint16_t); /// count elements
		for (auto iter = strMap.begin(); iter != strMap.end(); ++iter)
			size += (sizeof(uint32_t) + iter->first.size()) + (sizeof(uint32_t) + iter->second.size());
		return size;
	}

	std::map<std::string, std::string> strMap;


	MapMessage& add(const std::string& key, const std::string& value)
	{
		strMap[key] = value;
		return *this;
	}

	std::string get(const std::string& key, const std::string& def = "")
	{
		if (strMap.find(key) == strMap.end())
			return def;

		return strMap[key];
	}


	template<class T>
	T get(const std::string& key, const T& def)
	{
		T result;
		std::stringstream defaultValue;
		defaultValue << def;
		std::string value = get(key, defaultValue.str());

		std::istringstream is(value);
		while (is.good())
		{
			if (is.peek() != EOF)
				is >> result;
			else
				break;
		}
		return result;
	}


protected:
	virtual void doserialize(std::ostream& stream) const
	{
		uint16_t size = strMap.size();
		writeVal(stream, size);
		for (auto iter = strMap.begin(); iter != strMap.end(); ++iter)
		{
			writeVal(stream, iter->first);
			writeVal(stream, iter->second);
		}
	}
};

}


#endif


