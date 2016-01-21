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

#ifndef STRING_MESSAGE_H
#define STRING_MESSAGE_H

#include "message.h"
#include <string>


namespace msg
{

class StringMessage : public BaseMessage
{
public:
	StringMessage(message_type msgType) : BaseMessage(msgType), str("")
	{
	}

	StringMessage(message_type msgType, const std::string& _str) : BaseMessage(msgType), str(_str)
	{
	}

	virtual ~StringMessage()
	{
	}

	virtual void read(std::istream& stream)
	{
		int16_t size;
		stream.read(reinterpret_cast<char *>(&size), sizeof(int16_t));
		str.resize(size);
		stream.read(&str[0], size);
	}

	virtual uint32_t getSize() const
	{
		return sizeof(int16_t) + str.size();
	}

	std::string str;

protected:
	virtual void doserialize(std::ostream& stream) const
	{
		int16_t size(str.size());
		stream.write(reinterpret_cast<const char *>(&size), sizeof(int16_t));
		stream.write(str.c_str(), size);
	}
};

}


#endif


