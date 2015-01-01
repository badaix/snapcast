#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include <iostream>
#include <streambuf>
#include <vector>
#include <sys/time.h>


template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class vectorwrapbuf : public std::basic_streambuf<CharT, TraitsT>
{
public:
	vectorwrapbuf(std::vector<CharT> &vec)
	{
		this->setg(vec.data(), vec.data(), vec.data() + vec.size());
	}
};


struct membuf : public std::basic_streambuf<char>
{
	membuf(char* begin, char* end)
	{
		this->setg(begin, begin, end);
	}
};


enum message_type
{
	kBase = 0,
	kHeader = 1,
	kWireChunk = 2,
	kSampleFormat = 3,
	kServerSettings = 4,
	kTime = 5,
	kRequest = 6,
	kAck = 7,
	kCommand = 8
};



struct tv
{
	tv()
	{
		timeval t;
		gettimeofday(&t, NULL);
		sec = t.tv_sec;
		usec = t.tv_usec;
	}
	tv(timeval tv) : sec(tv.tv_sec), usec(tv.tv_usec) {};
	tv(int32_t _sec, int32_t _usec) : sec(_sec), usec(_usec) {};

	int32_t sec;
	int32_t usec;
	/*
	5.3 - 6.2 = -0.9
	-1
	0.1

	5.3 - 6.4 = -1.1
	-1
	-0.1
	*/
//(timeMsg.received.sec - timeMsg.sent.sec) * 1000000 + (timeMsg.received.usec - timeMsg.sent.usec)
	tv operator-(const tv& other)
	{
		tv result(*this);
		result.sec -= other.sec;
		result.usec -= other.usec;
		if (result.usec > 0)
		{
			result.sec += 1;
			result.usec = 1000000 - result.usec;
		}
		else if (result.usec < 0)
		{
			result.usec *= -1;
		}
		/*		else if (result.usec >= 1000000)
				{
					result.usec -= 1000000;
					result.sec += 1;
				}
		*/		return result;
	}
};

namespace msg
{

struct BaseMessage
{
	BaseMessage() : type(kBase), id(0), refersTo(0)
	{
	}

	BaseMessage(message_type type_) : type(type_), id(0), refersTo(0)
	{
	}

	virtual ~BaseMessage()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char*>(&type), sizeof(uint16_t));
		stream.read(reinterpret_cast<char*>(&id), sizeof(uint16_t));
		stream.read(reinterpret_cast<char*>(&refersTo), sizeof(uint16_t));
		stream.read(reinterpret_cast<char *>(&sent.sec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&sent.usec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&received.sec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&received.usec), sizeof(int32_t));
		stream.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
	}

	void deserialize(char* payload)
	{
		membuf databuf(payload, payload + BaseMessage::getSize());
		std::istream is(&databuf);
		read(is);
	}

	void deserialize(const BaseMessage& baseMessage, char* payload)
	{
		type = baseMessage.type;
		id = baseMessage.id;
		refersTo = baseMessage.refersTo;
		sent = baseMessage.sent;
		received = baseMessage.received;
		size = baseMessage.size;
		membuf databuf(payload, payload + size);
		std::istream is(&databuf);
		read(is);
	}

	virtual void serialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char*>(&type), sizeof(uint16_t));
		stream.write(reinterpret_cast<char*>(&id), sizeof(uint16_t));
		stream.write(reinterpret_cast<char*>(&refersTo), sizeof(uint16_t));
		stream.write(reinterpret_cast<char *>(&sent.sec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&sent.usec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&received.sec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&received.usec), sizeof(int32_t));
		size = getSize();
		stream.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
		doserialize(stream);
	}

	virtual uint32_t getSize()
	{
		return 3*sizeof(uint16_t) + 2*sizeof(tv) + sizeof(uint32_t);
	};

	uint16_t type;
	uint16_t id;
	uint16_t refersTo;
	tv sent;
	tv received;
	uint32_t size;

protected:
	virtual void doserialize(std::ostream& stream)
	{
	};
};


struct SerializedMessage
{
	~SerializedMessage()
	{
		free(buffer);
	}

	BaseMessage message;
	char* buffer;
};

}

#endif


