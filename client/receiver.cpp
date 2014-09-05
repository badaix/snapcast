#include "receiver.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "common/log.h"
#include "oggDecoder.h"
#include "pcmDecoder.h"


#define PCM_DEVICE "default"

using namespace std;


Receiver::Receiver(Stream* stream) : active_(false), stream_(stream)
{
}


void Receiver::socketRead(tcp::socket* socket, void* to, size_t bytes)
{
	size_t toRead = bytes;
	size_t len = 0;
	do 
	{
		len += socket->read_some(boost::asio::buffer((char*)to + len, toRead));
		toRead = bytes - len;
	}
	while (toRead > 0);
}


void Receiver::start(const std::string& ip, int port)
{
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(tcp::v4(), ip, boost::lexical_cast<string>(port));
	iterator = resolver.resolve(query);
	receiverThread = new thread(&Receiver::worker, this);
}


void Receiver::stop()
{
	active_ = false;
}


BaseMessage* Receiver::getNextMessage(tcp::socket* socket)
{
	BaseMessage baseMessage;
	size_t baseMsgSize = baseMessage.getSize();
	vector<char> buffer(baseMsgSize);

	socketRead(socket, &buffer[0], baseMsgSize);
	baseMessage.readVec(buffer);
//cout << "type: " << baseMessage.type << ", size: " << baseMessage.size << "\n";
	if (baseMessage.size > buffer.size())
		buffer.resize(baseMessage.size);
	socketRead(socket, &buffer[0], baseMessage.size);
	BaseMessage* message = NULL;
	if (baseMessage.type == message_type::payload)
		message = new PcmChunk(stream_->format, 0);
	else if (baseMessage.type == message_type::header)
		message = new HeaderMessage();
	if (message != NULL)
		message->readVec(buffer);
	return message;
}


void Receiver::worker() 
{
	active_ = true;
	OggDecoder decoder;
	while (active_)
	{
		try
		{
			tcp::socket s(io_service);
			s.connect(*iterator);
			struct timeval tv;
			tv.tv_sec  = 5; 
			tv.tv_usec = 0;         
			setsockopt(s.native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
//			std::clog << kLogNotice << "connected to " << ip << ":" << port << std::endl;

			while(active_)
			{
				BaseMessage* message = getNextMessage(&s);
				if (message == NULL)
					continue;

				if (message->type == message_type::payload)
				{
					if (decoder.decode((PcmChunk*)message))
						stream_->addChunk((PcmChunk*)message);
					else
						delete message;
//cout << ", decoded: " << chunk->payloadSize << ", Duration: " << chunk->getDuration() << ", sec: " << chunk->tv_sec << ", usec: " << chunk->tv_usec/1000 << ", type: " << chunk->type << "\n";
				}
				else if (message->type == message_type::header)
				{
					decoder.setHeader((HeaderMessage*)message);
				}
			}
		}
		catch (const std::exception& e)
		{
			cout << kLogNotice << "Exception: " << e.what() << ", trying to reconnect" << std::endl;
			stream_->clearChunks();
			usleep(500*1000);
		}
	}
}



