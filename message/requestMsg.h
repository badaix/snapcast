#ifndef REQUEST_MSG_H
#define REQUEST_MSG_H

#include "message.h"
#include <string>


class RequestMsg : public BaseMessage
{
public:
	RequestMsg() : BaseMessage(message_type::requestmsg), request(base)
	{
	}

	RequestMsg(message_type _request) : BaseMessage(message_type::requestmsg), request(_request)
	{
	}

	virtual ~RequestMsg()
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




#endif


