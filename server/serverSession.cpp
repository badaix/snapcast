#include "serverSession.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <mutex>
#include "common/log.h"

using namespace std;




ServerSession::ServerSession(MessageReceiver* _receiver, std::shared_ptr<tcp::socket> _socket) : messageReceiver(_receiver)
{
	socket = _socket;
}


ServerSession::~ServerSession()
{
	stop();
}


void ServerSession::start()
{
	active_ = true;
	streamActive = false;
	readerThread = new thread(&ServerSession::reader, this);
	writerThread = new thread(&ServerSession::writer, this);
}


void ServerSession::stop()
{
	active_ = false;
	try
	{
		boost::system::error_code ec;
		if (socket)
		{
			socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
			if (ec) logE << "Error in socket shutdown: " << ec << "\n";
			socket->close(ec);
			if (ec) logE << "Error in socket close: " << ec << "\n";
		}
		if (readerThread)
		{
			logD << "joining readerThread\n";
			readerThread->join();
			delete readerThread;
		}
		if (writerThread)
		{
			logD << "joining readerThread\n";
			writerThread->join();
			delete writerThread;
		}
	}
	catch(...)
	{
	}
	readerThread = NULL;
	writerThread = NULL;
	logD << "ServerSession stopped\n";
}



void ServerSession::socketRead(void* _to, size_t _bytes)
{
	size_t read = 0;
	do
	{
		boost::system::error_code error;
		read += socket->read_some(boost::asio::buffer((char*)_to + read, _bytes - read));
	}
	while (read < _bytes);
}


void ServerSession::add(shared_ptr<msg::BaseMessage> message)
{
	if (!message || !streamActive)
		return;

	while (messages.size() > 100)// chunk->getDuration() > 10000)
		messages.pop();
	messages.push(message);
}


bool ServerSession::send(msg::BaseMessage* message)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	if (!socket)
		return false;
	boost::asio::streambuf streambuf;
	std::ostream stream(&streambuf);
	tv t;
	message->sent = t;
	message->serialize(stream);
	boost::asio::write(*socket.get(), streambuf);
	return true;
}


void ServerSession::getNextMessage()
{
//logD << "getNextMessage\n";
	msg::BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
//logD << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(&buffer[0], baseMessage.size);
	tv t;
	baseMessage.received = t;

	if (messageReceiver != NULL)
		messageReceiver->onMessageReceived(this, baseMessage, &buffer[0]);
}



void ServerSession::reader()
{
	active_ = true;
	try
	{
		while (active_)
		{
			getNextMessage();
		}
	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception: " << e.what() << ", trying to reconnect" << endl;
	}
	active_ = false;
}




void ServerSession::writer()
{
	try
	{
		boost::asio::streambuf streambuf;
		std::ostream stream(&streambuf);
		shared_ptr<msg::BaseMessage> message;
		while (active_)
		{
			if (messages.try_pop(message, std::chrono::milliseconds(500)))
				send(message.get());
		}
	}
	catch (std::exception& e)
	{
		logE << "Exception in thread: " << e.what() << "\n";
	}
	active_ = false;
}










