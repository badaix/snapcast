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

#ifndef SERVER_SETTINGS_H
#define SERVER_SETTINGS_H

#include "message/message.h"


namespace msg
{

class ServerSettings : public BaseMessage
{
public:
	ServerSettings() : BaseMessage(message_type::kServerSettings)
	{
	}

	virtual ~ServerSettings()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&bufferMs), sizeof(int32_t));
	}

	virtual uint32_t getSize()
	{
		return sizeof(int32_t);
	}

	int32_t bufferMs;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&bufferMs), sizeof(int32_t));
	}
};

}


#endif


