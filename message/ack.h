#ifndef ACK_MSG_H
#define ACK_MSG_H

#include "message.h"

namespace msg
{

class Ack : public BaseMessage
{
public:
	Ack() : BaseMessage(message_type::kAck), message("")
	{
	}

	Ack(const std::string& _message) : BaseMessage(message_type::kAck), message(_message)
	{
	}

	virtual ~Ack()
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

}



#endif


