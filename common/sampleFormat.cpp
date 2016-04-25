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

#include <vector>
#include <iostream>
#include <sstream>

#include "sampleFormat.h"
#include "common/strCompat.h"
#include "common/utils.h"
#include "common/log.h"


using namespace std;


SampleFormat::SampleFormat()
{
}


SampleFormat::SampleFormat(const std::string& format)
{
	setFormat(format);
}


SampleFormat::SampleFormat(uint32_t sampleRate, uint16_t bitsPerSample, uint16_t channelCount)
{
	setFormat(sampleRate, bitsPerSample, channelCount);
}


string SampleFormat::getFormat() const
{
	stringstream ss;
	ss << rate << ":" << bits << ":" << channels;
	return ss.str();
}


void SampleFormat::setFormat(const std::string& format)
{
	std::vector<std::string> strs;
	strs = split(format, ':');
	if (strs.size() == 3)
		setFormat(
		    cpt::stoul(strs[0]),
		    cpt::stoul(strs[1]),
		    cpt::stoul(strs[2]));
}


void SampleFormat::setFormat(uint32_t rate, uint16_t bits, uint16_t channels)
{
	//needs something like:
	// 24_4 = 3 bytes, padded to 4
	// 32 = 4 bytes
	this->rate = rate;
	this->bits = bits;
	this->channels = channels;
	sampleSize = bits / 8;
	if (bits == 24)
		sampleSize = 4;
	frameSize = channels*sampleSize;
//	logD << "SampleFormat: " << rate << ":" << bits << ":" << channels << "\n";
}


