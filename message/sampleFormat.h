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

#ifndef SAMPLE_FORMAT_H
#define SAMPLE_FORMAT_H

#include <string>
#include "message.h"

namespace msg
{

class SampleFormat : public BaseMessage
{
public:
	SampleFormat();
	SampleFormat(const std::string& format);
	SampleFormat(uint32_t rate, uint16_t bits, uint16_t channels);

	void setFormat(const std::string& format);
	void setFormat(uint32_t rate, uint16_t bits, uint16_t channels);

	uint32_t rate;
	uint16_t bits;
	uint16_t channels;

	uint16_t sampleSize;
	uint16_t frameSize;

	inline double msRate() const
	{
		return (double)rate/1000.;
	}

	inline double usRate() const
	{
		return (double)rate/1000000.;
	}

	inline double nsRate() const
	{
		return (double)rate/1000000000.;
	}

	virtual void read(std::istream& stream)
	{
		stream.read(reinterpret_cast<char *>(&rate), sizeof(uint32_t));
		stream.read(reinterpret_cast<char *>(&bits), sizeof(uint16_t));
		stream.read(reinterpret_cast<char *>(&channels), sizeof(uint16_t));
		stream.read(reinterpret_cast<char *>(&sampleSize), sizeof(uint16_t));
		stream.read(reinterpret_cast<char *>(&frameSize), sizeof(uint16_t));
	}

	virtual uint32_t getSize()
	{
		return sizeof(int32_t) + 4*sizeof(int16_t);
	}

protected:
	virtual void doserialize(std::ostream& stream)
	{
		stream.write(reinterpret_cast<char *>(&rate), sizeof(uint32_t));
		stream.write(reinterpret_cast<char *>(&bits), sizeof(uint16_t));
		stream.write(reinterpret_cast<char *>(&channels), sizeof(uint16_t));
		stream.write(reinterpret_cast<char *>(&sampleSize), sizeof(uint16_t));
		stream.write(reinterpret_cast<char *>(&frameSize), sizeof(uint16_t));
	}

};

}

#endif

