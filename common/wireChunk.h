#ifndef WIRE_CHUNK_H
#define WIRE_CHUNK_H

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <streambuf>
#include <vector>
#include "message.h"



class WireChunk : public BaseMessage
{
public:
	WireChunk(size_t size = 0) : BaseMessage(message_type::payload), payloadSize(size)
	{
		payload = (char*)malloc(size);
	}

	virtual ~WireChunk()
	{
		free(payload);
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&tv_sec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&tv_usec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		payload = (char*)realloc(payload, payloadSize);
		stream.read(payload, payloadSize);
	}

	virtual uint32_t getSize()
	{
		return sizeof(int32_t) + sizeof(int32_t) + sizeof(uint32_t) + payloadSize;
	}

	int32_t tv_sec;
	int32_t tv_usec;
	uint32_t payloadSize;
	char* payload;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&tv_sec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&tv_usec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};




#endif


