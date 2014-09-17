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




#endif


