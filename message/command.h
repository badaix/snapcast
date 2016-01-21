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

#ifndef COMMAND_MSG_H
#define COMMAND_MSG_H

#include "stringMessage.h"
#include <string>


namespace msg
{

class Command : public StringMessage
{
public:
	Command() : StringMessage(message_type::kCommand)
	{
	}

	Command(const std::string& _command) : StringMessage(message_type::kCommand, _command)
	{
	}

	virtual ~Command()
	{
	}


	const std::string& getCommand() const
	{
		return str;
	}

	void setCommand(const std::string& command)
	{
		str = command;
	}

};

}


#endif


