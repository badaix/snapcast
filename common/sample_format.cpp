/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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

#include <iostream>
#include <sstream>
#include <vector>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils.hpp"
#include "common/utils/string_utils.hpp"
#include "sample_format.hpp"


using namespace std;


SampleFormat::SampleFormat()
{
    setFormat(0, 0, 0);
}


SampleFormat::SampleFormat(const std::string& format)
{
    setFormat(format);
}


SampleFormat::SampleFormat(uint32_t sampleRate, uint16_t bitsPerSample, uint16_t channels)
{
    setFormat(sampleRate, bitsPerSample, channels);
}


string SampleFormat::toString() const
{
    stringstream ss;
    ss << rate_ << ":" << bits_ << ":" << channels_;
    return ss.str();
}


void SampleFormat::setFormat(const std::string& format)
{
    std::vector<std::string> strs;
    strs = utils::string::split(format, ':');
    if (strs.size() == 3)
        setFormat(strs[0] == "*" ? 0 : cpt::stoul(strs[0]), strs[1] == "*" ? 0 : static_cast<uint16_t>(cpt::stoul(strs[1])),
                  strs[2] == "*" ? 0 : static_cast<uint16_t>(cpt::stoul(strs[2])));
    else
        throw SnapException("sampleformat must be <rate>:<bits>:<channels>");
}


void SampleFormat::setFormat(uint32_t rate, uint16_t bits, uint16_t channels)
{
    // needs something like:
    // 24_4 = 3 bytes, padded to 4
    // 32 = 4 bytes
    rate_ = rate;
    bits_ = bits;
    channels_ = channels;
    sample_size_ = bits / 8;
    if (bits_ == 24)
        sample_size_ = 4;
    frame_size_ = channels_ * sample_size_;
    //	LOG(DEBUG) << "SampleFormat: " << rate << ":" << bits << ":" << channels << "\n";
}
