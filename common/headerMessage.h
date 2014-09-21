#ifndef HEADER_MESSAGE_H
#define HEADER_MESSAGE_H

#include "common/message.h"



class HeaderMessage : public BaseMessage
{
public:
	HeaderMessage(const std::string& codecName = "", size_t size = 0) : BaseMessage(message_type::header), payloadSize(size), codec(codecName)
	{
		payload = (char*)malloc(size);
	}

	virtual ~HeaderMessage()
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

	virtual uint32_t getSize()
	{
		return sizeof(int16_t) + codec.size() + sizeof(uint32_t) + payloadSize;
	}

	uint32_t payloadSize;
	char* payload;
	std::string codec;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		int16_t size(codec.size());
		stream.write(reinterpret_cast<char *>(&size), sizeof(int16_t));
		stream.write(codec.c_str(), size);
		stream.write(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};



#endif


