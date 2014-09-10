#include "controlServer.h"
#include <iostream>


ControlServer::ControlServer(unsigned short port) : port_(port), headerChunk(NULL)
{
}


void ControlServer::onMessageReceived(SocketConnection* connection, const BaseMessage& baseMessage, char* buffer)
{
	cout << "onMessageReceived: " << baseMessage.type << ", " << baseMessage.size << "\n";
}


void ControlServer::acceptor()
{
	tcp::acceptor a(io_service_, tcp::endpoint(tcp::v4(), port_));
	for (;;)
	{
		socket_ptr sock(new tcp::socket(io_service_));
		a.accept(*sock);
		cout << "New connection: " << sock->remote_endpoint().address().to_string() << "\n";
		ServerConnection* session = new ServerConnection(this, sock);
		sessions.insert(shared_ptr<ServerConnection>(session));
		session->start();
	}
}


void ControlServer::start()
{
	acceptThread = new thread(&ControlServer::acceptor, this);
}


void ControlServer::stop()
{
//		acceptThread->join();
}



