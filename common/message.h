#ifndef CHUNK_H
#define CHUNK_H

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "common/sampleFormat.h"


typedef std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::milliseconds> time_point_ms;
using namespace std;


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
		type = type;
	};

	virtual void read(istream& stream)
	{
		stream.read(reinterpret_cast<char*>(&type), sizeof(uint16_t));
		cout << type << "\n";
		stream.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
		cout << size << "\n";
	}

	virtual void serialize(ostream& stream)
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
	virtual void doserialize(ostream& stream)
	{
	};
};



class TestMessage : public BaseMessage 
{
public:
	TestMessage() : BaseMessage(message_type::header)
	{
	}

	TestMessage(int8_t logLevel_, char* text_) : BaseMessage(message_type::header), logLevel(logLevel_), length(sizeof(text_)), text(text_)
	{		
	}

	virtual ~TestMessage()
	{
	}

	virtual void read(istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&logLevel), sizeof(int8_t));
		stream.read(reinterpret_cast<char *>(&length), sizeof(int16_t));
		text = (char*)malloc(length);
		stream.read(text, length);
	}

	virtual uint32_t getSize()
	{
		return sizeof(int8_t) + sizeof(int16_t) + length;
	}

	int8_t logLevel;
	int16_t length;
	char* text;

protected:
	virtual void doserialize(ostream& stream)
	{
		stream.write(reinterpret_cast<char*>(&logLevel), sizeof(int8_t));
		stream.write(reinterpret_cast<char*>(&length), sizeof(int16_t));
		stream.write(text, length);
	}
};


class WireChunk : public BaseMessage 
{
public:
	WireChunk() : BaseMessage(message_type::payload)
	{
	}

//	WireChunk(int8_t logLevel_, char* text_) : BaseMessage(message_type::payload), logLevel(logLevel_), length(sizeof(text_)), text(text_)
//	{		
//	}

	virtual ~WireChunk()
	{
	}

	virtual void read(istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&tv_sec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&tv_usec), sizeof(int32_t));
		stream.read(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		payload = (char*)malloc(payloadSize);
		stream.read(payload, payloadSize);
	}

	virtual uint32_t getSize()
	{
		return sizeof(int32_t) + sizeof(int32_t) + sizeof(uint32_t) + payloadSize;
	}

	int32_t tv_sec;
	int32_t tv_usec;
	uint32_t payloadSize;
	char* payload;

protected:
	virtual void doserialize(ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&tv_sec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&tv_usec), sizeof(int32_t));
		stream.write(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};



class HeaderMessage : public BaseMessage 
{
public:
	HeaderMessage(size_t size = 0) : BaseMessage(message_type::payload), payloadSize(size)
	{
		payload = (char*)malloc(size);
	}

	virtual ~HeaderMessage()
	{
	}

	virtual void read(istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		payload = (char*)malloc(payloadSize);
		stream.read(payload, payloadSize);
	}

	virtual uint32_t getSize()
	{
		return sizeof(uint32_t) + payloadSize;
	}

	uint32_t payloadSize;
	char* payload;

protected:
	virtual void doserialize(ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&payloadSize), sizeof(uint32_t));
		stream.write(payload, payloadSize);
	}
};




/*

	virtual ~BaseMessage(){};
	virtual message_type getType() = 0;

	virtual char* serialize()
	{
		char* outStream = (char*)malloc(getSize());
		size_t pos = 0;
		pos = add(outStream, pos, getType());
		pos = add(outStream, pos, getSize());
		doSerialize(outStream + pos);
		return outStream;
	}
	
	virtual void deserialize(istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&drvId), sizeof(int));
		stream.read(reinterpret_cast<char *>(&frameSize), sizeof(int));
		msg = (byte*)malloc((sizeof(byte)) * frameSize);
		stream.read(reinterpret_cast<char *>(&msg[0]), frameSize);		
		stream.read(reinterpret_cast<char *>(&offset), sizeof(unsigned int));
		stream.read(reinterpret_cast<char *>(&isLive), sizeof(bool));

		size_t pos = 0;
		memcpy((char*)&type, inStream, sizeof(uint16_t));
		pos += sizeof(uint16_t);
		memcpy(&length, inStream + pos, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		doDeserialize(inStream + pos);
	}

	virtual size_t getSize() 
	{
		return sizeof(int16_t) + sizeof(int32_t) + doGetSize();
	}

protected:
	template <typename T>
	size_t add(char* stream, size_t pos, T* t)
	{
		memcpy(stream + pos, reinterpret_cast<char*>(t), sizeof(T));
		return pos + sizeof(T);
	}

	virtual size_t doGetSize() = 0;
	virtual void doSerialize(char* outStream) = 0;
	virtual void doDeserialize(const char* inStream) = 0;
};


struct WireChunk : public BaseMessage
{
	int32_t tv_sec;
	int32_t tv_usec;
	uint32_t payloadSize;
	char* payload;

	virtual ~WireChunk()
	{
		free(payload);
	}

	virtual message_type getType()
	{
		return message_type::payload;
	}

protected:
	virtual size_t doGetSize()
	{
		return sizeof(int32_t) + sizeof(int32_t) + sizeof(uint32_t) + payloadSize;
	}

	virtual void doSerialize(char* outStream)
	{
		size_t pos = 0;
		pos = add(outStream, pos, tv_sec);
		pos = add(outStream, pos, tv_usec);
		pos = add(outStream, pos, payloadSize);
		memcpy(outStream + pos, payload, payloadSize);
	}

	virtual void doDeserialize(const char* inStream)
	{
		size_t pos = 0;
		memcpy(&tv_sec, inStream + pos, sizeof(int32_t));
		pos += sizeof(int32_t);
		memcpy(&tv_usec, inStream + pos, sizeof(int32_t));
		pos += sizeof(int32_t);
		memcpy(&payloadSize, inStream + pos, sizeof(uint32_t));
		pos += sizeof(uint32_t);
		payload = (char*)malloc(payloadSize);
		memcpy(payload, inStream + pos, payloadSize);
	}
};
*/

class PcmChunk : public WireChunk
{
public:
	PcmChunk(const SampleFormat& sampleFormat, size_t ms);
	~PcmChunk();

	int read(void* outputBuffer, size_t frameCount);
	bool isEndOfChunk() const;

	inline time_point_ms timePoint() const
	{
		time_point_ms tp;
		std::chrono::milliseconds::rep relativeIdxTp = ((double)idx / ((double)format.rate/1000.));
		return 
			tp + 
			std::chrono::seconds(tv_sec) + 
			std::chrono::milliseconds(tv_usec / 1000) + 
			std::chrono::milliseconds(relativeIdxTp);
	}

	template<typename T>
	inline T getAge() const
	{
		return getAge<T>(timePoint());
	}

	inline long getAge() const
	{
		return getAge<std::chrono::milliseconds>().count();
	}

	inline static long getAge(const time_point_ms& time_point)
	{
		return getAge<std::chrono::milliseconds>(time_point).count();
	}

	template<typename T, typename U>
	static inline T getAge(const std::chrono::time_point<U>& time_point)
	{
		return std::chrono::duration_cast<T>(std::chrono::high_resolution_clock::now() - time_point);
	}

	int seek(int frames);
	double getDuration() const;
	double getDurationUs() const;
	double getTimeLeft() const;
	double getFrameCount() const;

	SampleFormat format;

private:
//	SampleFormat format_;
	uint32_t idx;
};



#endif


