#ifndef SAMPLE_FORMAT
#define SAMPLE_FORMAT

#include <string>


class SampleFormat
{
public:
	SampleFormat();
	SampleFormat(const std::string& format);
	SampleFormat(uint16_t rate = 48000, uint16_t bits = 16, uint16_t channels = 2);

	void setFormat(const std::string& format);
	void setFormat(uint16_t rate, uint16_t bits, uint16_t channels);

	const uint16_t& rate;
	const uint16_t& bits;
	const uint16_t& channels;

	const uint16_t& sampleSize;
	const uint16_t& frameSize;

	float msRate() const { return (float)rate/1000.f; }

private:
	uint16_t rate_;
	uint16_t bits_;
	uint16_t channels_;
	uint16_t bytes_;
	uint16_t frameSize_;
};


#endif

