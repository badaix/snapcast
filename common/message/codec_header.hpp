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

#pragma once


// local headers
#include "message.hpp"

namespace msg
{

/**
 * Codec dependend header of encoded data stream
 */
class CodecHeader : public BaseMessage
{
public:
    /// c'tor taking the @p codec_name and @p site of the payload
    explicit CodecHeader(const std::string& codec_name = "", uint32_t size = 0)
        : BaseMessage(message_type::kCodecHeader), payloadSize(size), payload(nullptr), codec(codec_name)
    {
        if (size > 0)
            payload = static_cast<char*>(malloc(size * sizeof(char)));
    }

    /// d'tor
    ~CodecHeader() override
    {
        free(payload);
    }

    void read(std::istream& stream) override
    {
        readVal(stream, codec);
        readVal(stream, &payload, payloadSize);
    }

    uint32_t getSize() const override
    {
        return static_cast<uint32_t>(sizeof(uint32_t) + codec.size() + sizeof(uint32_t) + payloadSize);
    }

    /// payload size
    uint32_t payloadSize;
    /// the payload
    char* payload;
    /// name of the codec
    std::string codec;

protected:
    void doserialize(std::ostream& stream) const override
    {
        writeVal(stream, codec);
        writeVal(stream, payload, payloadSize);
    }
};

} // namespace msg
