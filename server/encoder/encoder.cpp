/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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


// prototype/interface header file
#include "encoder.hpp"

// local headers

// standard headers


namespace encoder
{

Encoder::Encoder(std::string codecOptions) : headerChunk_(nullptr), codecOptions_(std::move(codecOptions))
{
}


void Encoder::init(OnEncodedCallback callback, const SampleFormat& format)
{
    if (codecOptions_.empty())
        codecOptions_ = getDefaultOptions();
    encoded_callback_ = std::move(callback);
    sampleFormat_ = format;
    initEncoder();
}


std::string Encoder::getAvailableOptions() const
{
    return "No codec options supported";
}


std::string Encoder::getDefaultOptions() const
{
    return "";
}


std::shared_ptr<msg::CodecHeader> Encoder::getHeader() const
{
    return headerChunk_;
}


void Encoder::setStreamTimestamp(std::chrono::time_point<std::chrono::steady_clock> timestamp)
{
    tvEncodedChunk_ = timestamp;
}


void Encoder::chunkEncoded(std::shared_ptr<msg::PcmChunk> chunk, double duration_ms)
{
    // absolute start timestamp is the tvEncodedChunk_
    auto microsecs = std::chrono::duration_cast<std::chrono::microseconds>(tvEncodedChunk_.time_since_epoch()).count();
    chunk->timestamp.sec = microsecs / 1000000;
    chunk->timestamp.usec = microsecs % 1000000;

    // update tvEncodedChunk_ to the next chunk start by adding the current chunk duration
    tvEncodedChunk_ += std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(duration_ms * 1000000));

    if (encoded_callback_)
        encoded_callback_(*this, std::move(chunk), duration_ms);
}


} // namespace encoder
