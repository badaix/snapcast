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

#ifndef CODEC_HEADER_MESSAGE_H
#define CODEC_HEADER_MESSAGE_H

#include "message.h"

namespace msg
{

/**
 * Codec dependend header of encoded data stream
 */
class CodecHeader : public BaseMessage
{
public:
	CodecHeader(const std::string& codecName = "", size_t size = 0) : BaseMessage(message_type::kCodecHeader), payloadSize(size), codec(codecName)
	{
		payload = (char*)malloc(size);
	}

	virtual ~CodecHeader()
	{
		free(payload);
	}

	virtual void read(std::istream& stream)
	{
		readVal(stream, codec);
		readVal(stream, &payload, payloadSize);
	}

	virtual uint32_t getSize() const
	{
		return sizeof(uint32_t) + codec.size() + sizeof(uint32_t) + payloadSize;
	}

	uint32_t payloadSize;
	char* payload;
	std::string codec;

protected:
	virtual void doserialize(std::ostream& stream) const
	{
		writeVal(stream, codec);
		writeVal(stream, payload, payloadSize);
	}
};

}


#endif


