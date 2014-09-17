#ifndef SERVER_CONNECTION_H
#define SERVER_CONNECTION_H

#include "common/socketConnection.h"


using boost::asio::ip::tcp;




class ServerConnection : public SocketConnection
{
public:
	ServerConnection(MessageReceiver* _receiver, std::shared_ptr<tcp::socket> _socket);

protected:
	virtual void worker();
};




#endif




