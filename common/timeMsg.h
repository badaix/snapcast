#ifndef TIME_MSG_H
#define TIME_MSG_H

#include "message.h"


class TimeMsg : public BaseMessage
{
public:
	TimeMsg() : BaseMessage(message_type::timemsg)
	{
	}

	virtual ~TimeMsg()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&latency), sizeof(int32_t));
	}

	virtual uint32_t getSize()
	{
		return sizeof(int32_t);
	}

	uint32_t latency;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&latency), sizeof(int32_t));
	}
};




#endif


