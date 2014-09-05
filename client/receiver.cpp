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
			BaseMessage baseMessage;
			size_t baseMsgSize = baseMessage.getSize();
			vector<char> buffer(baseMsgSize);
			while (true)
			{
				socketRead(&s, &buffer[0], baseMsgSize);
				baseMessage.readVec(buffer);
//cout << "type: " << baseMessage.type << ", size: " << baseMessage.size << "\n";
				if (baseMessage.size > buffer.size())
					buffer.resize(baseMessage.size);
				socketRead(&s, &buffer[0], baseMessage.size);
				if (baseMessage.type == message_type::payload)
				{
					PcmChunk* chunk = new PcmChunk(stream_->format, 0);
					chunk->readVec(buffer);
//cout << "WireChunk length: " << chunk->payloadSize;
					if (decoder.decode(chunk))
						stream_->addChunk(chunk);
//cout << ", decoded: " << chunk->payloadSize << ", Duration: " << chunk->getDuration() << ", sec: " << chunk->tv_sec << ", usec: " << chunk->tv_usec/1000 << ", type: " << chunk->type << "\n";
				}
				else if (baseMessage.type == message_type::header)
				{
					HeaderMessage* chunk = new HeaderMessage();
					chunk->readVec(buffer);
					decoder.setHeader(chunk);
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



