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

#ifndef HEADER_MESSAGE_H
#define HEADER_MESSAGE_H

#include "message.h"

namespace msg
{

/**
 * Codec dependend header of encoded data stream
 */
class Header : public BaseMessage
{
public:
	Header(const std::string& codecName = "", size_t size = 0) : BaseMessage(message_type::kHeader), payloadSize(size), codec(codecName)
	{
		payload = (char*)malloc(size);
	}

	virtual ~Header()
	{
		free(payload);
	}

	virtual void read(std::istream& stream)
	{
		int16_t size;
		stream.read(reinterpret_cast<char *>(&size), sizeof(int16_t));
		codec.resize(size);
		stream.read(&codec[0], size);

		stream.read(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		payload = (char*)realloc(payload, payloadSize);
		stream.read(payload, payloadSize);
	}

	virtual uint32_t getSize() const
	{
		return sizeof(int16_t) + codec.size() + sizeof(uint32_t) + payloadSize;
	}

	uint32_t payloadSize;
	char* payload;
	std::string codec;

protected:
	virtual void doserialize(std::ostream& stream) const
	{
		int16_t size(codec.size());
		stream.write(reinterpret_cast<const char *>(&size), sizeof(int16_t));
		stream.write(codec.c_str(), size);
		stream.write(reinterpret_cast<const char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};

}


#endif


