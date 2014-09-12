#include "controlServer.h"
#include <iostream>


ControlServer::ControlServer(unsigned short port) : port_(port), headerChunk(NULL), sampleFormat(NULL)
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
		cout << "ControlServer::NewConnection: " << sock->remote_endpoint().address().to_string() << "\n";
		ServerConnection* session = new ServerConnection(this, sock);
		sessions.insert(shared_ptr<ServerConnection>(session));
		session->start();
		session->send(serverSettings);
		session->send(sampleFormat);
		session->send(headerChunk);
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


void ControlServer::setHeader(HeaderMessage* header)
{
	if (header)
		headerChunk = header;
}


void ControlServer::setFormat(SampleFormat* format)
{
	if (format)
		sampleFormat = format;
}



void ControlServer::setServerSettings(ServerSettings* settings)
{
	if (settings)
		serverSettings = settings;
}





