/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <streambuf>
#include <vector>
#include <sys/time.h>
#include "common/endian.h"


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
	kCodecHeader = 1,
	kWireChunk = 2,
	kServerSettings = 3,
	kTime = 4,
	kHello = 5
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

	tv operator+(const tv& other) const
	{
		tv result(*this);
		result.sec += other.sec;
		result.usec += other.usec;
		if (result.usec > 1000000)
		{
			result.sec += result.usec / 1000000;
			result.usec %= 1000000;
		}
		return result;
	}

	tv operator-(const tv& other) const
	{
		tv result(*this);
		result.sec -= other.sec;
		result.usec -= other.usec;
		while (result.usec < 0)
		{
			result.sec -= 1;
			result.usec += 1000000;
		}
		return result;
	}
};

namespace msg
{

const size_t max_size = 1000000;

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
		readVal(stream, type);
		readVal(stream, id);
		readVal(stream, refersTo);
		readVal(stream, sent.sec);
		readVal(stream, sent.usec);
		readVal(stream, received.sec);
		readVal(stream, received.usec);
		readVal(stream, size);
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

	virtual void serialize(std::ostream& stream) const
	{
		writeVal(stream, type);
		writeVal(stream, id);
		writeVal(stream, refersTo);
		writeVal(stream, sent.sec);
		writeVal(stream, sent.usec);
		writeVal(stream, received.sec);
		writeVal(stream, received.usec);
		size = getSize();
		writeVal(stream, size);
		doserialize(stream);
	}

	virtual uint32_t getSize() const
	{
		return 3*sizeof(uint16_t) + 2*sizeof(tv) + sizeof(uint32_t);
	};

	uint16_t type;
	mutable uint16_t id;
	uint16_t refersTo;
	tv received;
	mutable tv sent;
	mutable uint32_t size;

protected:
	void writeVal(std::ostream& stream, const bool& val) const
	{
		char c = val?1:0;
		writeVal(stream, c);
	}

	void writeVal(std::ostream& stream, const char& val) const
	{
		stream.write(reinterpret_cast<const char*>(&val), sizeof(char));
	}

	void writeVal(std::ostream& stream, const uint16_t& val) const
	{
		uint16_t v = SWAP_16(val);
		stream.write(reinterpret_cast<const char*>(&v), sizeof(uint16_t));
	}

	void writeVal(std::ostream& stream, const int16_t& val) const
	{
		uint16_t v = SWAP_16(val);
		stream.write(reinterpret_cast<const char*>(&v), sizeof(int16_t));
	}

	void writeVal(std::ostream& stream, const uint32_t& val) const
	{
		uint32_t v = SWAP_32(val);
		stream.write(reinterpret_cast<const char*>(&v), sizeof(uint32_t));
	}

	void writeVal(std::ostream& stream, const int32_t& val) const
	{
		uint32_t v = SWAP_32(val);
		stream.write(reinterpret_cast<const char*>(&v), sizeof(int32_t));
	}

	void writeVal(std::ostream& stream, const char* payload, const uint32_t& size) const
	{
		writeVal(stream, size);
		stream.write(payload, size);
	}

	void writeVal(std::ostream& stream, const std::string& val) const
	{
		uint32_t size = val.size();
		writeVal(stream, val.c_str(), size);
	}



	void readVal(std::istream& stream, bool& val) const
	{
		char c;
		readVal(stream, c);
		val = (c != 0);
	}

	void readVal(std::istream& stream, char& val) const
	{
		stream.read(reinterpret_cast<char*>(&val), sizeof(char));
	}

	void readVal(std::istream& stream, uint16_t& val) const
	{
		stream.read(reinterpret_cast<char*>(&val), sizeof(uint16_t));
		val = SWAP_16(val);
	}

	void readVal(std::istream& stream, int16_t& val) const
	{
		stream.read(reinterpret_cast<char*>(&val), sizeof(int16_t));
		val = SWAP_16(val);
	}

	void readVal(std::istream& stream, uint32_t& val) const
	{
		stream.read(reinterpret_cast<char*>(&val), sizeof(uint32_t));
		val = SWAP_32(val);
	}

	void readVal(std::istream& stream, int32_t& val) const
	{
		stream.read(reinterpret_cast<char*>(&val), sizeof(int32_t));
		val = SWAP_32(val);
	}

	void readVal(std::istream& stream, char** payload, uint32_t& size) const
	{
		readVal(stream, size);
		*payload = (char*)realloc(*payload, size);
		stream.read(*payload, size);
	}

	void readVal(std::istream& stream, std::string& val) const
	{
		uint32_t size;
		readVal(stream, size);
		val.resize(size);
		stream.read(&val[0], size);
	}


	virtual void doserialize(std::ostream& stream) const
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


