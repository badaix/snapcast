#include "controlServer.h"
#include "common/timeMsg.h"
#include "common/requestMsg.h"
#include <iostream>


ControlServer::ControlServer(unsigned short port) : port_(port), headerChunk(NULL), sampleFormat(NULL)
{
}




void ControlServer::send(shared_ptr<BaseMessage> message)
{
	for (std::set<shared_ptr<ServerSession>>::iterator it = sessions.begin(); it != sessions.end(); )
	{
		if (!(*it)->active())
		{
			cout << "Session inactive. Removing\n";
			sessions.erase(it++);
		}
		else
			++it;
	}

	for (auto s : sessions)
		s->add(message);
}



void ControlServer::onMessageReceived(ServerSession* connection, const BaseMessage& baseMessage, char* buffer)
{
//	cout << "onMessageReceived: " << baseMessage.type << ", size: " << baseMessage.size << ", sent: " << baseMessage.sent.sec << "," << baseMessage.sent.usec << ", recv: " << baseMessage.received.sec << "," << baseMessage.received.usec << "\n";
	if (baseMessage.type == message_type::requestmsg)
	{
		RequestMsg requestMsg;
		requestMsg.deserialize(baseMessage, buffer);
		cout << "request: " << requestMsg.request << "\n";
		if (requestMsg.request == "time")
		{
//		timeMsg.latency = (timeMsg.received.sec - timeMsg.sent.sec) * 1000000 + (timeMsg.received.usec - timeMsg.sent.usec);
			TimeMsg timeMsg;
			timeMsg.refersTo = requestMsg.id;
			timeMsg.latency = (requestMsg.received.sec - requestMsg.sent.sec) + (requestMsg.received.usec - requestMsg.sent.usec) / 1000000.;
//		tv diff = timeMsg.received - timeMsg.sent;
//		cout << "Latency: " << diff.sec << "." << diff.usec << "\n";
			connection->send(&timeMsg);
		}
		else if (requestMsg.request == "serverSettings")
		{
			serverSettings->refersTo = requestMsg.id;
			connection->send(serverSettings);
		}
		else if (requestMsg.request == "sampleFormat")
		{
			sampleFormat->refersTo = requestMsg.id;
			connection->send(sampleFormat);
		}
		else if (requestMsg.request == "headerChunk")
		{
			headerChunk->refersTo = requestMsg.id;
			connection->send(headerChunk);
		}
	}
}


void ControlServer::acceptor()
{
	tcp::acceptor a(io_service_, tcp::endpoint(tcp::v4(), port_));
	for (;;)
	{
		socket_ptr sock(new tcp::socket(io_service_));
		struct timeval tv;
		tv.tv_sec  = 5;
		tv.tv_usec = 0;
		a.accept(*sock);
		setsockopt(sock->native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		setsockopt(sock->native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
		cout << "ControlServer::NewConnection: " << sock->remote_endpoint().address().to_string() << "\n";
		ServerSession* session = new ServerSession(this, sock);
		sessions.insert(shared_ptr<ServerSession>(session));
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





