#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstring>
#include <iostream>
#include <streambuf>
#include <vector>



template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class vectorwrapbuf : public std::basic_streambuf<CharT, TraitsT> {
public:
    vectorwrapbuf(std::vector<CharT> &vec) {
        this->setg(vec.data(), vec.data(), vec.data() + vec.size());
    }
};


struct membuf : public std::basic_streambuf<char>
{
    membuf(char* begin, char* end) {
        this->setg(begin, begin, end);
    }
};


enum message_type
{
	base = 0,
	header = 1,
	payload = 2,
	sampleformat = 3
};



struct BaseMessage
{
	BaseMessage() : type(base)
	{
	}

	BaseMessage(message_type type_) : type(type_) 
	{
	}

	virtual ~BaseMessage()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char*>(&type), sizeof(uint16_t));
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
		size = baseMessage.size;
		membuf databuf(payload, payload + size);
		std::istream is(&databuf);
		read(is);
	}

	virtual void serialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char*>(&type), sizeof(uint16_t));
		size = getSize();
		stream.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
		doserialize(stream);
	}

	virtual uint32_t getSize()
	{
		return sizeof(uint16_t) + sizeof(uint32_t);
	};

	uint16_t type;
	uint32_t size;
protected:
	virtual void doserialize(std::ostream& stream)
	{
	};
};


#endif


