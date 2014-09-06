#ifndef HEADER_MESSAGE_H
#define HEADER_MESSAGE_H

#include "common/message.h"



class HeaderMessage : public BaseMessage 
{
public:
	HeaderMessage(size_t size = 0) : BaseMessage(message_type::header), payloadSize(size)
	{
		payload = (char*)malloc(size);
	}

	virtual ~HeaderMessage()
	{
		free(payload);
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		payload = (char*)realloc(payload, payloadSize);
		stream.read(payload, payloadSize);
	}

	virtual uint32_t getSize()
	{
		return sizeof(uint32_t) + payloadSize;
	}

	uint32_t payloadSize;
	char* payload;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};



#endif


