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
			while (true)
			{
				WireChunk* wireChunk = new WireChunk();
				socketRead(&s, wireChunk, Chunk::getHeaderSize());
				wireChunk->payload = (char*)malloc(wireChunk->length);
				socketRead(&s, wireChunk->payload, wireChunk->length);
				Chunk* chunk = new Chunk(stream_->format, wireChunk);
//cout << "WireChunk length: " << wireChunk->length << ", Duration: " << chunk->getDuration() << ", sec: " << wireChunk->tv_sec << ", usec: " << wireChunk->tv_usec/1000 << ", type: " << wireChunk->type << "\n";
				if (decoder.decode(chunk))
				{
//cout << "Duration: " << chunk->getDuration() << "\n";
					stream_->addChunk(chunk);
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



