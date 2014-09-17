#ifndef REQUEST_MSG_H
#define REQUEST_MSG_H

#include "message.h"
#include <string>


class RequestMsg : public BaseMessage
{
public:
    RequestMsg() : BaseMessage(message_type::requestmsg)
    {
    }

    RequestMsg(const std::string& _request) : BaseMessage(message_type::requestmsg), request(_request)
    {
    }

    virtual ~RequestMsg()
    {
    }

    virtual void read(std::istream& stream)
    {
        int16_t size;
        stream.read(reinterpret_cast<char *>(&size), sizeof(int16_t));
        request.resize(size);
        stream.read(&request[0], size);
    }

    virtual uint32_t getSize()
    {
        return sizeof(int16_t) + request.size();
    }

    std::string request;

protected:
    virtual void doserialize(std::ostream& stream)
    {
        int16_t size(request.size());
        stream.write(reinterpret_cast<char *>(&size), sizeof(int16_t));
        stream.write(request.c_str(), size);
    }
};




#endif


