/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#include <vector>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

#include "sampleFormat.h"
#include "common/log.h"


namespace msg
{

SampleFormat::SampleFormat() : BaseMessage(message_type::kSampleFormat)
{
}


SampleFormat::SampleFormat(const std::string& format) : BaseMessage(message_type::kSampleFormat)
{
	setFormat(format);
}


SampleFormat::SampleFormat(uint32_t sampleRate, uint16_t bitsPerSample, uint16_t channelCount) : BaseMessage(message_type::kSampleFormat)
{
	setFormat(sampleRate, bitsPerSample, channelCount);
}


void SampleFormat::setFormat(const std::string& format)
{
	std::vector<std::string> strs;
	boost::split(strs, format, boost::is_any_of(":"));
	if (strs.size() == 3)
		setFormat(
		    boost::lexical_cast<uint32_t>(strs[0]),
		    boost::lexical_cast<uint16_t>(strs[1]),
		    boost::lexical_cast<uint16_t>(strs[2]));
}


void SampleFormat::setFormat(uint32_t rate, uint16_t bits, uint16_t channels)
{
	this->rate = rate;
	this->bits = bits;
	this->channels = channels;
	sampleSize = bits / 8;
	if (bits == 24)
		sampleSize = 4;
	frameSize = channels*sampleSize;
	logD << "SampleFormat: " << rate << ":" << bits << ":" << channels << "\n";
}

}



