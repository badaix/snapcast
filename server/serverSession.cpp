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


void ServerSession::start()
{
	active_ = true;
	readerThread = new thread(&ServerSession::reader, this);
	writerThread = new thread(&ServerSession::writer, this);
}


void ServerSession::socketRead(void* _to, size_t _bytes)
{
	size_t read = 0;
	do
	{
		boost::system::error_code error;
		read += socket->read_some(boost::asio::buffer((char*)_to + read, _bytes - read), error);
	}
	while (read < _bytes);
}


void ServerSession::add(shared_ptr<BaseMessage> message)
{
	if (!message)
		return;

	while (messages.size() > 100)// chunk->getDuration() > 10000)
		messages.pop();
	messages.push(message);
}


bool ServerSession::send(BaseMessage* message)
{
//	std::unique_lock<std::mutex> mlock(mutex_);
//cout << "send: " << message->type << ", size: " << message->getSize() << "\n";
	if (!connected())
		return false;
//cout << "send: " << message->type << ", size: " << message->getSize() << "\n";
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
//cout << "getNextMessage\n";
	BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);
	socketRead(&buffer[0], baseMsgSize);
	baseMessage.deserialize(&buffer[0]);
//cout << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " << baseMessage.refersTo << "\n";
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
		cout << kLogNotice << "Exception: " << e.what() << ", trying to reconnect" << std::endl;
	}
	active_ = false;
}




void ServerSession::writer()
{
	try
	{
		boost::asio::streambuf streambuf;
		std::ostream stream(&streambuf);
		for (;;)
		{
			shared_ptr<BaseMessage> message(messages.pop());
			send(message.get());
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception in thread: " << e.what() << "\n";
		active_ = false;
	}
}










