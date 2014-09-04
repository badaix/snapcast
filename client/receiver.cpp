#include "receiver.h"
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "common/log.h"
//#include "oggDecoder.h"
//#include "pcmDecoder.h"


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
//	OggDecoder decoder;
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
				BaseMessage baseMessage;
				boost::asio::streambuf b;
				// reserve 512 bytes in output sequence
				boost::asio::streambuf::mutable_buffers_type bufs = b.prepare(baseMessage.getSize());
				size_t read = s.receive(bufs);
//cout << "read: " << read << "\n";
				// received data is "committed" from output sequence to input sequence
				b.commit(baseMessage.getSize());
				std::istream is(&b);
				baseMessage.read(is);
//				cout << "type: " << baseMessage.type << ", size: " << baseMessage.size << "\n";

				read = 0;
				bufs = b.prepare(baseMessage.size);
				while (read < baseMessage.size)
				{
					size_t n = s.receive(bufs);
					b.commit(n);
					read += n;
				}
				// received data is "committed" from output sequence to input sequence
//				std::istream is(&b);
				if (baseMessage.type == message_type::payload)
				{
					PcmChunk* chunk = new PcmChunk(stream_->format, 0);
					chunk->read(is);
//cout << "WireChunk length: " << chunk->payloadSize << ", Duration: " << chunk->getDuration() << ", sec: " << chunk->tv_sec << ", usec: " << chunk->tv_usec/1000 << ", type: " << chunk->type << "\n";
					stream_->addChunk(chunk);
				}

//cout << "WireChunk length: " << wireChunk->length << ", Duration: " << chunk->getDuration() << ", sec: " << wireChunk->tv_sec << ", usec: " << wireChunk->tv_usec/1000 << ", type: " << wireChunk->type << "\n";
/*				if (decoder.decode(chunk))
				{
//cout << "Duration: " << chunk->getDuration() << "\n";
					stream_->addChunk(chunk);
				}
*/			}
		}
		catch (const std::exception& e)
		{
			cout << kLogNotice << "Exception: " << e.what() << ", trying to reconnect" << std::endl;
			stream_->clearChunks();
			usleep(500*1000);
		}
	}
}



