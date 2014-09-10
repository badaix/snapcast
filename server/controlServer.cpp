#include "controlServer.h"


ControlSession::ControlSession(socket_ptr sock) : active_(false), socket_(sock)
{
}


void ControlSession::sender()
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

void ControlSession::start()
{
	active_ = true;
	senderThread = new thread(&ControlSession::sender, this);
//		readerThread.join();
}

void ControlSession::send(BaseMessage* message)
{
	boost::asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	message->serialize(stream);
	boost::asio::write(*socket_, streambuf);
}


bool ControlSession::isActive() const
{
	return active_;
}




ControlServer::ControlServer(unsigned short port) : port_(port), headerChunk(NULL)
{
}


void ControlServer::acceptor()
{
	tcp::acceptor a(io_service_, tcp::endpoint(tcp::v4(), port_));
	for (;;)
	{
		socket_ptr sock(new tcp::socket(io_service_));
		a.accept(*sock);
		cout << "New connection: " << sock->remote_endpoint().address().to_string() << "\n";
		ControlSession* session = new ControlSession(sock);
		sessions.insert(shared_ptr<ControlSession>(session));
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



