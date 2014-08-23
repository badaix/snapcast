#ifndef SAMPLE_FORMAT_H
#define SAMPLE_FORMAT_H

#include <string>


class SampleFormat
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

/*private:
	uint16_t rate_;
	uint16_t bits_;
	uint16_t channels_;
	uint16_t bytes_;
	uint16_t frameSize_;
*/
};


#endif

