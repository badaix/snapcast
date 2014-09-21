#ifndef ACK_MSG_H
#define ACK_MSG_H

#include "message.h"


class AckMsg : public BaseMessage
{
public:
	AckMsg() : BaseMessage(message_type::ackMsg)
	{
	}

	virtual ~AckMsg()
	{
	}

	virtual void read(std::istream& stream)
	{
//		stream.read(reinterpret_cast<char *>(&latency), sizeof(double));
	}

	virtual uint32_t getSize()
	{
		return 0;//sizeof(double);
	}

//	double latency;

protected:
	virtual void doserialize(std::ostream& stream)
	{
//		stream.write(reinterpret_cast<char *>(&latency), sizeof(double));
	}
};




#endif


