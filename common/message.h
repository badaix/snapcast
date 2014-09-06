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



enum message_type
{
	header = 0,
	payload = 1
};



struct BaseMessage
{
	BaseMessage()
	{
	}

	BaseMessage(message_type type_)
	{
		type = type_;
	};

	virtual ~BaseMessage()
	{
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char*>(&type), sizeof(uint16_t));
		stream.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
	}

	virtual void readVec(std::vector<char>& stream)
	{
		vectorwrapbuf<char> databuf(stream);
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


