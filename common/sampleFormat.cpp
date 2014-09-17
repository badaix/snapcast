#include "sampleFormat.h"
#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>


SampleFormat::SampleFormat() : BaseMessage(message_type::sampleformat)
{
}


SampleFormat::SampleFormat(const std::string& format) : BaseMessage(message_type::sampleformat)
{
    setFormat(format);
}


SampleFormat::SampleFormat(uint16_t sampleRate, uint16_t bitsPerSample, uint16_t channelCount) : BaseMessage(message_type::sampleformat)
{
    setFormat(sampleRate, bitsPerSample, channelCount);
}


void SampleFormat::setFormat(const std::string& format)
{
    std::vector<std::string> strs;
    boost::split(strs, format, boost::is_any_of(":"));
    if (strs.size() == 3)
        setFormat(
            boost::lexical_cast<uint16_t>(strs[0]),
            boost::lexical_cast<uint16_t>(strs[1]),
            boost::lexical_cast<uint16_t>(strs[2]));
}


void SampleFormat::setFormat(uint16_t rate, uint16_t bits, uint16_t channels)
{
    this->rate = rate;
    this->bits = bits;
    this->channels = channels;
    sampleSize = bits / 8;
    if (bits == 24)
        sampleSize = 4;
    frameSize = channels*sampleSize;
}



