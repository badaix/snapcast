#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include "common/socketConnection.h"


using boost::asio::ip::tcp;



class ClientConnection : public SocketConnection
{
public:
    ClientConnection(MessageReceiver* _receiver, const std::string& _ip, size_t _port);
    virtual void start();

protected:
    virtual void worker();

private:
    std::string ip;
    size_t port;
};




#endif




