#ifndef ACK_MSG_H
#define ACK_MSG_H

#include "message.h"


class AckMsg : public BaseMessage
{
public:
	AckMsg() : BaseMessage(message_type::ackMsg), message("")
	{
	}

	AckMsg(const std::string& _message) : BaseMessage(message_type::requestmsg), message(_message)
	{
	}

	virtual ~AckMsg()
	{
	}

	virtual void read(std::istream& stream)
	{
		int16_t size;
		stream.read(reinterpret_cast<char *>(&size), sizeof(int16_t));
		message.resize(size);
		stream.read(&message[0], size);
	}

	virtual uint32_t getSize()
	{
		return sizeof(int16_t) + message.size();
	}

	std::string message;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		int16_t size(message.size());
		stream.write(reinterpret_cast<char *>(&size), sizeof(int16_t));
		stream.write(message.c_str(), size);
	}
};




#endif


