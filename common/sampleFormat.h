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

#ifndef SAMPLE_FORMAT_H
#define SAMPLE_FORMAT_H

#include <string>


/**
 * sample and frame as defined in alsa:
 * http://www.alsa-project.org/main/index.php/FramesPeriods
 * Say we want to work with a stereo, 16-bit, 44.1 KHz stream, one-way (meaning, either in playback or in capture direction). Then we have:
 * 'stereo' = number of channels: 2
 * 1 analog sample is represented with 16 bits = 2 bytes
 * 1 frame represents 1 analog sample from all channels; here we have 2 channels, and so:
 * 1 frame = (num_channels) * (1 sample in bytes) = (2 channels) * (2 bytes (16 bits) per sample) = 4 bytes (32 bits)
 * To sustain 2x 44.1 KHz analog rate - the system must be capable of data transfer rate, in Bytes/sec:
 * Bps_rate = (num_channels) * (1 sample in bytes) * (analog_rate) = (1 frame) * (analog_rate) = ( 2 channels ) * (2 bytes/sample) * (44100 samples/sec) = 2*2*44100 = 176400 Bytes/sec (link to formula img)
 */
class SampleFormat
{
public:
	SampleFormat();
	SampleFormat(const std::string& format);
	SampleFormat(uint32_t rate, uint16_t bits, uint16_t channels);

	std::string getFormat() const;

	void setFormat(const std::string& format);
	void setFormat(uint32_t rate, uint16_t bits, uint16_t channels);

	uint32_t rate;
	uint16_t bits;
	uint16_t channels;

	// size in [bytes] of a single mono sample, e.g. 2 bytes (= 16 bits)
	uint16_t sampleSize;

	// size in [bytes] of a frame (sum of sample sizes = #channel*sampleSize), e.g. 4 bytes (= 2 channel * 16 bit)
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
};


#endif

