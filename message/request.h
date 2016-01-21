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

#ifndef REQUEST_MSG_H
#define REQUEST_MSG_H

#include "message.h"
#include <string>

namespace msg
{

/**
 * Request is sent from client to server. The answer is identified by a request id
 */
class Request : public BaseMessage
{
public:
	Request() : BaseMessage(message_type::kRequest), request(kBase)
	{
	}

	Request(message_type _request) : BaseMessage(message_type::kRequest), request(_request)
	{
	}

	virtual ~Request()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&request), sizeof(int16_t));
	}

	virtual uint32_t getSize() const
	{
		return sizeof(int16_t);
	}

	message_type request;

protected:
	virtual void doserialize(std::ostream& stream) const
	{
		stream.write(reinterpret_cast<const char *>(&request), sizeof(int16_t));
	}
};

}


#endif


