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
#include "json_message.hpp"


namespace msg
{

/// Dynamic settings that affect the client
class ServerSettings : public JsonMessage
{
public:
    /// c'tor
    ServerSettings() : JsonMessage(message_type::kServerSettings)
    {
        setBufferMs(0);
        setLatency(0);
        setVolume(100);
        setMuted(false);
    }

    /// d'tor
    ~ServerSettings() override = default;

    /// @return the end to end delay in [ms]
    int32_t getBufferMs()
    {
        return get("bufferMs", 0);
    }

    /// @return client specific additional latency in [ms]
    int32_t getLatency()
    {
        return get("latency", 0);
    }

    /// @return the volume in [%]
    uint16_t getVolume()
    {
        return get("volume", static_cast<uint16_t>(100));
    }

    /// @return if muted
    bool isMuted()
    {
        return get("muted", false);
    }


    /// Set the end to end delay to @p buffer_ms [ms]
    void setBufferMs(int32_t buffer_ms)
    {
        msg["bufferMs"] = buffer_ms;
    }

    /// Set the additional client specific @p latency [ms]
    void setLatency(int32_t latency)
    {
        msg["latency"] = latency;
    }

    /// Set the @p volume [%]
    void setVolume(uint16_t volume)
    {
        msg["volume"] = volume;
    }

    /// Set client to @p muted
    void setMuted(bool muted)
    {
        msg["muted"] = muted;
    }
};

} // namespace msg
