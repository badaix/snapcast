/***
        This file is part of snapcast
        Copyright (C) 2015  Hannes Ellinger

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

#ifndef OPUS_ENCODER_HPP
#define OPUS_ENCODER_HPP

#include "common/resampler.hpp"
#include "encoder.hpp"
#include <opus/opus.h>


namespace encoder
{

class OpusEncoder : public Encoder
{
public:
    OpusEncoder(const std::string& codecOptions = "");
    ~OpusEncoder() override;

    void encode(const msg::PcmChunk& chunk) override;
    std::string getAvailableOptions() const override;
    std::string getDefaultOptions() const override;
    std::string name() const override;

protected:
    void encode(const SampleFormat& format, const char* data, size_t size);
    void initEncoder() override;
    ::OpusEncoder* enc_;
    std::vector<unsigned char> encoded_;
    std::unique_ptr<msg::PcmChunk> remainder_;
    size_t remainder_max_size_;
    std::unique_ptr<Resampler> resampler_;
};

} // namespace encoder

#endif
