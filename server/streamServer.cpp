#include "streamServer.h"



StreamSession::StreamSession(socket_ptr sock) : active_(false), socket_(sock)
{
}


void StreamSession::sender()
{
	try
	{
		boost::asio::streambuf streambuf;
		std::ostream stream(&streambuf);
		for (;;)
		{
			shared_ptr<BaseMessage> message(messages.pop());
			message->serialize(stream);
			boost::asio::write(*socket_, streambuf);
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception in thread: " << e.what() << "\n";
		active_ = false;
	}
}

void StreamSession::start()
{
	active_ = true;
	senderThread = new thread(&StreamSession::sender, this);
//		readerThread.join();
}

void StreamSession::send(shared_ptr<BaseMessage> message)
{
	if (!message)
		return;

	while (messages.size() > 100)//* chunk->getDuration() > 10000)
		messages.pop();
	messages.push(message);
}

bool StreamSession::isActive() const
{
	return active_;
}



StreamServer::StreamServer(unsigned short port) : port_(port), headerChunk(NULL)
{
}


void StreamServer::acceptor()
{
	tcp::acceptor a(io_service_, tcp::endpoint(tcp::v4(), port_));
	for (;;)
	{
		socket_ptr sock(new tcp::socket(io_service_));
		a.accept(*sock);
		cout << "New connection: " << sock->remote_endpoint().address().to_string() << "\n";
		StreamSession* session = new StreamSession(sock);
		session->send(sampleFormat);
		session->send(headerChunk);
		session->start();
		sessions.insert(shared_ptr<StreamSession>(session));
	}
}


void StreamServer::setHeader(shared_ptr<HeaderMessage> header)
{
	if (header)
		headerChunk = header;
}


void StreamServer::setFormat(SampleFormat& format)
{
	sampleFormat = shared_ptr<SampleFormat>(new SampleFormat(format));
}


void StreamServer::send(shared_ptr<BaseMessage> message)
{
	for (std::set<shared_ptr<StreamSession>>::iterator it = sessions.begin(); it != sessions.end(); ) 
	{
		if (!(*it)->isActive())
		{
			cout << "Session inactive. Removing\n";
	        sessions.erase(it++);
		}
	    else
	        ++it;
    }

	for (auto s : sessions)
		s->send(message);
}


void StreamServer::start()
{
	acceptThread = new thread(&StreamServer::acceptor, this);
}


void StreamServer::stop()
{
//		acceptThread->join();
}



