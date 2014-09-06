#ifndef SAMPLE_FORMAT_H
#define SAMPLE_FORMAT_H

#include <string>
#include "message.h"


class SampleFormat : public BaseMessage
{
public:
	SampleFormat();
	SampleFormat(const std::string& format);
	SampleFormat(uint16_t rate, uint16_t bits, uint16_t channels);

	void setFormat(const std::string& format);
	void setFormat(uint16_t rate, uint16_t bits, uint16_t channels);

	uint16_t rate;
	uint16_t bits;
	uint16_t channels;

	uint16_t sampleSize;
	uint16_t frameSize;

	float msRate() const { return (float)rate/1000.f; }

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&rate), sizeof(uint16_t));
		stream.read(reinterpret_cast<char *>(&bits), sizeof(uint16_t));
		stream.read(reinterpret_cast<char *>(&channels), sizeof(uint16_t));
		stream.read(reinterpret_cast<char *>(&sampleSize), sizeof(uint16_t));
		stream.read(reinterpret_cast<char *>(&frameSize), sizeof(uint16_t));
	}

	virtual uint32_t getSize()
	{
		return 5*sizeof(int16_t);
	}

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&rate), sizeof(uint16_t));
		stream.write(reinterpret_cast<char *>(&bits), sizeof(uint16_t));
		stream.write(reinterpret_cast<char *>(&channels), sizeof(uint16_t));
		stream.write(reinterpret_cast<char *>(&sampleSize), sizeof(uint16_t));
		stream.write(reinterpret_cast<char *>(&frameSize), sizeof(uint16_t));
	}

/*private:
	uint16_t rate_;
	uint16_t bits_;
	uint16_t channels_;
	uint16_t bytes_;
	uint16_t frameSize_;
*/
};


#endif

