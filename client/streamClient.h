#ifndef STREAM_CLIENT_H
#define STREAM_CLIENT_H

#include "clientConnection.h"


using boost::asio::ip::tcp;


class StreamClient : public ClientConnection
{
public:
    StreamClient(MessageReceiver* _receiver, const std::string& _ip, size_t _port);
    virtual ~StreamClient();
};



#endif




