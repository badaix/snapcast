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

#ifndef HELLO_MSG_H
#define HELLO_MSG_H

#include "message.h"
#include <string>


namespace msg
{

class Hello : public BaseMessage
{
public:
	Hello() : BaseMessage(message_type::kHello), macAddress("")
	{
	}

	Hello(const std::string& _macAddress) : BaseMessage(message_type::kHello), macAddress(_macAddress)
	{
	}

	virtual ~Hello()
	{
	}

	virtual void read(std::istream& stream)
	{
		int16_t size;
		stream.read(reinterpret_cast<char *>(&size), sizeof(int16_t));
		macAddress.resize(size);
		stream.read(&macAddress[0], size);
	}

	virtual uint32_t getSize() const
	{
		return sizeof(int16_t) + macAddress.size();
	}

	std::string macAddress;

protected:
	virtual void doserialize(std::ostream& stream) const
	{
		int16_t size(macAddress.size());
		stream.write(reinterpret_cast<const char *>(&size), sizeof(int16_t));
		stream.write(macAddress.c_str(), size);
	}
};

}


#endif


