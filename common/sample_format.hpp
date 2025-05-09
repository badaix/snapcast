/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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

#pragma once

// standard headers
#include <cstdint>
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
 * Bps_rate = (num_channels) * (1 sample in bytes) * (analog_rate) = (1 frame) * (analog_rate) = ( 2 channels ) * (2 bytes/sample) * (44100 samples/sec) =
 * 2*2*44100 = 176400 Bytes/sec (link to formula img)
 */
class SampleFormat
{
public:
    /// c'tor
    SampleFormat();
    /// c'tor
    SampleFormat(const std::string& format);
    /// c'tor
    SampleFormat(uint32_t rate, uint16_t bits, uint16_t channels);

    /// @return sampleformat as string rate:bits::channels
    std::string toString() const;

    /// Set @p format (rate:bits::channels)
    void setFormat(const std::string& format);
    /// Set format
    void setFormat(uint32_t rate, uint16_t bits, uint16_t channels);

    /// @return if has format
    bool isInitialized() const
    {
        return ((rate_ != 0) || (bits_ != 0) || (channels_ != 0));
    }

    /// @return rate
    uint32_t rate() const
    {
        return rate_;
    }

    /// @return bits
    uint16_t bits() const
    {
        return bits_;
    }

    /// @return channels
    uint16_t channels() const
    {
        return channels_;
    }

    /// @return size in [bytes] of a single mono sample, e.g. 2 bytes (= 16 bits)
    uint16_t sampleSize() const
    {
        return sample_size_;
    }

    /// @return size in [bytes] of a frame (sum of sample sizes = num-channel*sampleSize), e.g. 4 bytes (= 2 channel * 16 bit)
    uint16_t frameSize() const
    {
        return frame_size_;
    }

    /// @return rate per ms (= rate / 1000)
    inline double msRate() const
    {
        return static_cast<double>(rate_) / 1000.;
    }

    /// @return rate per micro seconds (= rate / 1000000)
    inline double usRate() const
    {
        return static_cast<double>(rate_) / 1000000.;
    }

    /// @return rate per nano seconds (= rate / 1000000000)
    inline double nsRate() const
    {
        return static_cast<double>(rate_) / 1000000000.;
    }

private:
    uint16_t sample_size_;
    uint16_t frame_size_;
    uint32_t rate_;
    uint16_t bits_;
    uint16_t channels_;
};
