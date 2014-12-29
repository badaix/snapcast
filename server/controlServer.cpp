#include "controlServer.h"
#include "message/time.h"
#include "message/ack.h"
#include "message/request.h"
#include "message/command.h"
#include <iostream>


ControlServer::ControlServer(unsigned short port) : port_(port), headerChunk(NULL), sampleFormat(NULL)
{
}




void ControlServer::send(shared_ptr<msg::BaseMessage> message)
{
	std::unique_lock<std::mutex> mlock(mutex);
	for (std::set<shared_ptr<ServerSession>>::iterator it = sessions.begin(); it != sessions.end(); )
	{
		if (!(*it)->active())
		{
			cout << "Session inactive. Removing\n";
			(*it)->stop();
			sessions.erase(it++);
		}
		else
			++it;
	}

	for (auto s : sessions)
		s->add(message);
}


void ControlServer::onMessageReceived(ServerSession* connection, const msg::BaseMessage& baseMessage, char* buffer)
{
//	cout << "onMessageReceived: " << baseMessage.type << ", size: " << baseMessage.size << ", sent: " << baseMessage.sent.sec << "," << baseMessage.sent.usec << ", recv: " << baseMessage.received.sec << "," << baseMessage.received.usec << "\n";
	if (baseMessage.type == message_type::kRequest)
	{
		msg::Request requestMsg;
		requestMsg.deserialize(baseMessage, buffer);
//		cout << "request: " << requestMsg.request << "\n";
		if (requestMsg.request == kTime)
		{
//		timeMsg.latency = (timeMsg.received.sec - timeMsg.sent.sec) * 1000000 + (timeMsg.received.usec - timeMsg.sent.usec);
			msg::Time timeMsg;
			timeMsg.refersTo = requestMsg.id;
			timeMsg.latency = (requestMsg.received.sec - requestMsg.sent.sec) + (requestMsg.received.usec - requestMsg.sent.usec) / 1000000.;
//		tv diff = timeMsg.received - timeMsg.sent;
//		cout << "Latency: " << diff.sec << "." << diff.usec << "\n";
			connection->send(&timeMsg);
		}
		else if (requestMsg.request == kServerSettings)
		{
			serverSettings->refersTo = requestMsg.id;
			connection->send(serverSettings);
		}
		else if (requestMsg.request == kSampleFormat)
		{
			sampleFormat->refersTo = requestMsg.id;
			connection->send(sampleFormat);
		}
		else if (requestMsg.request == kHeader)
		{
			headerChunk->refersTo = requestMsg.id;
			connection->send(headerChunk);
		}
	}
	else if (baseMessage.type == message_type::kCommand)
	{
		msg::Command commandMsg;
		commandMsg.deserialize(baseMessage, buffer);
		if (commandMsg.command == "startStream")
		{
			msg::Ack ackMsg;
			ackMsg.refersTo = commandMsg.id;
			connection->send(&ackMsg);
			connection->setStreamActive(true);
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
		{
			std::unique_lock<std::mutex> mlock(mutex);
			session->start();
			sessions.insert(shared_ptr<ServerSession>(session));
		}
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


void ControlServer::setHeader(msg::Header* header)
{
	if (header)
		headerChunk = header;
}


void ControlServer::setFormat(msg::SampleFormat* format)
{
	if (format)
		sampleFormat = format;
}



void ControlServer::setServerSettings(msg::ServerSettings* settings)
{
	if (settings)
		serverSettings = settings;
}





