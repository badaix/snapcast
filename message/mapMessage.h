/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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
		stream.read(reinterpret_cast<char *>(&mapSize), sizeof(uint16_t));

		for (size_t n=0; n<mapSize; ++n)
		{
			uint16_t size;
			std::string key;
			stream.read(reinterpret_cast<char *>(&size), sizeof(uint16_t));
			key.resize(size);
			stream.read(&key[0], size);

			std::string value;
			stream.read(reinterpret_cast<char *>(&size), sizeof(uint16_t));
			value.resize(size);
			stream.read(&value[0], size);

			strMap[key] = value;
		}
	}

	virtual uint32_t getSize() const
	{
		uint32_t size = sizeof(uint16_t); /// count elements
		for (auto iter = strMap.begin(); iter != strMap.end(); ++iter)
			size += (sizeof(uint16_t) + iter->first.size()) + (sizeof(uint16_t) + iter->second.size());
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


protected:
	virtual void doserialize(std::ostream& stream) const
	{
		uint16_t size = strMap.size();
		stream.write(reinterpret_cast<const char *>(&size), sizeof(uint16_t));
		for (auto iter = strMap.begin(); iter != strMap.end(); ++iter)
		{
			size = iter->first.size();
			stream.write(reinterpret_cast<const char *>(&size), sizeof(uint16_t));
			stream.write(iter->first.c_str(), iter->first.size());
			size = iter->second.size();
			stream.write(reinterpret_cast<const char *>(&size), sizeof(uint16_t));
			stream.write(iter->second.c_str(), iter->second.size());
		}
	}
};

}


#endif


