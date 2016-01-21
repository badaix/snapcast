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

#ifndef WIRE_CHUNK_H
#define WIRE_CHUNK_H

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <streambuf>
#include <vector>
#include "message.h"
#include "common/timeDefs.h"


namespace msg
{

/**
 * Piece of raw data
 * Has information about "when" captured (timestamp)
 */
class WireChunk : public BaseMessage
{
public:
	WireChunk(size_t size = 0) : BaseMessage(message_type::kWireChunk), payloadSize(size)
	{
		payload = (char*)malloc(size);
	}

	WireChunk(const WireChunk& wireChunk) : BaseMessage(message_type::kWireChunk), timestamp(wireChunk.timestamp), payloadSize(wireChunk.payloadSize)
	{
		payload = (char*)malloc(payloadSize);
		memcpy(payload, wireChunk.payload, payloadSize);
	}

	virtual ~WireChunk()
	{
		free(payload);
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&timestamp.sec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&timestamp.usec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		payload = (char*)realloc(payload, payloadSize);
		stream.read(payload, payloadSize);
	}

	virtual uint32_t getSize() const
	{
		return sizeof(int32_t) + sizeof(int32_t) + sizeof(uint32_t) + payloadSize;
	}

	virtual chronos::time_point_hrc start() const
	{
		return chronos::time_point_hrc(chronos::sec(timestamp.sec) + chronos::usec(timestamp.usec));
	}

	tv timestamp;
	uint32_t payloadSize;
	char* payload;

protected:
	virtual void doserialize(std::ostream& stream) const
	{
		stream.write(reinterpret_cast<const char *>(&timestamp.sec), sizeof(int32_t));
		stream.write(reinterpret_cast<const char *>(&timestamp.usec), sizeof(int32_t));
		stream.write(reinterpret_cast<const char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};

}


#endif


