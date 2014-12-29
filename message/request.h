#ifndef REQUEST_MSG_H
#define REQUEST_MSG_H

#include "message.h"
#include <string>

namespace msg
{

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

	virtual uint32_t getSize()
	{
		return sizeof(int16_t);
	}

	message_type request;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&request), sizeof(int16_t));
	}
};

}


#endif


