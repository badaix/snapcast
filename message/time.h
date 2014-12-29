#ifndef TIME_MSG_H
#define TIME_MSG_H

#include "message.h"

namespace msg
{

class Time : public BaseMessage
{
public:
	Time() : BaseMessage(message_type::kTime)
	{
	}

	virtual ~Time()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&latency), sizeof(double));
	}

	virtual uint32_t getSize()
	{
		return sizeof(double);
	}

	double latency;

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&latency), sizeof(double));
	}
};

}


#endif


