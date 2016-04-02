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

#ifndef TIME_MSG_H
#define TIME_MSG_H

#include "message.h"

namespace msg
{

class Time : public BaseMessage
{
public:
	Time() : BaseMessage(message_type::kTime)
	{
	}

	virtual ~Time()
	{
	}

	virtual void read(std::istream& stream)
	{
		readVal(stream, latency.sec);
		readVal(stream, latency.usec);
	}

	virtual uint32_t getSize() const
	{
		return sizeof(tv);
	}

	tv latency;

protected:
	virtual void doserialize(std::ostream& stream) const
	{
		writeVal(stream, latency.sec);
		writeVal(stream, latency.usec);
	}
};

}


#endif


