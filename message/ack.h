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

#ifndef ACK_MSG_H
#define ACK_MSG_H

#include "message.h"

namespace msg
{

class Ack : public BaseMessage
{
public:
	Ack() : BaseMessage(message_type::kAck), message("")
	{
	}

	Ack(const std::string& _message) : BaseMessage(message_type::kAck), message(_message)
	{
	}

	virtual ~Ack()
	{
	}

	virtual void read(std::istream& stream)
	{
		int16_t size;
		stream.read(reinterpret_cast<char *>(&size), sizeof(int16_t));
		message.resize(size);
		stream.read(&message[0], size);
	}

	virtual uint32_t getSize()
	{
		return sizeof(int16_t) + message.size();
	}

	std::string message;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		int16_t size(message.size());
		stream.write(reinterpret_cast<char *>(&size), sizeof(int16_t));
		stream.write(message.c_str(), size);
	}
};

}



#endif


