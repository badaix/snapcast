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
#include "common/time_sync.hpp"

namespace msg
{

/// Time sync message, send from client to server and back
class Time : public BaseMessage
{
public:
    /// c'tor
    Time() : BaseMessage(message_type::kTime), version(static_cast<uint8_t>(time_sync::ProtocolVersion::V2)),
             source(static_cast<uint8_t>(time_sync::TimeSyncSource::NONE)), quality(0.5f), error_ms(50.0f)
    {
    }

    /// d'tor
    ~Time() override = default;

    void read(std::istream& stream) override
    {
        // Read latency (always present in all versions)
        readVal(stream, latency.sec);
        readVal(stream, latency.usec);
        
        // Check if we have more data (protocol V2+)
        if (stream.eof())
        {
            // Protocol V1 (legacy) - only latency
            version = static_cast<uint8_t>(time_sync::ProtocolVersion::V1);
            return;
        }
        
        // Protocol V2+ - read additional fields
        readVal(stream, version);
        readVal(stream, source);
        readVal(stream, quality);
        readVal(stream, error_ms);
    }

    uint32_t getSize() const override
    {
        if (version == static_cast<uint8_t>(time_sync::ProtocolVersion::V1))
        {
            // Protocol V1 (legacy) - only latency
            return sizeof(tv);
        }
        else
        {
            // Protocol V2+ - all fields
            return sizeof(tv) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(float) + sizeof(float);
        }
    }

    /// The latency after round trip "client => server => client"
    tv latency;
    
    /// Protocol version
    uint8_t version;
    
    /// Time source type
    uint8_t source;
    
    /// Time source quality (0.0-1.0)
    float quality;
    
    /// Estimated error in milliseconds
    float error_ms;

protected:
    void doserialize(std::ostream& stream) const override
    {
        // Write latency (always present in all versions)
        writeVal(stream, latency.sec);
        writeVal(stream, latency.usec);
        
        // Protocol V1 (legacy) - only latency
        if (version == static_cast<uint8_t>(time_sync::ProtocolVersion::V1))
        {
            return;
        }
        
        // Protocol V2+ - write additional fields
        writeVal(stream, version);
        writeVal(stream, source);
        writeVal(stream, quality);
        writeVal(stream, error_ms);
    }
};

} // namespace msg
