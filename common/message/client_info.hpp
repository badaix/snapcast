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

/// Client information sent from client to server
/// Might also be used for other things in future, like
/// - sync stats
/// - latency estimations
/// - Battery status
/// - ...
class ClientInfo : public JsonMessage
{
public:
    /// c'tor
    ClientInfo() : JsonMessage(message_type::kClientInfo)
    {
        setVolume(100);
        setMuted(false);
    }

    /// d'tor
    ~ClientInfo() override = default;

    /// @return the volume in percent
    uint16_t getVolume()
    {
        return get("volume", static_cast<uint16_t>(100));
    }

    /// @return if muted or not
    bool isMuted()
    {
        return get("muted", false);
    }

    /// Set the volume to @p volume percent
    void setVolume(uint16_t volume)
    {
        msg["volume"] = volume;
    }

    /// Set muted to @p muted
    void setMuted(bool muted)
    {
        msg["muted"] = muted;
    }
};

} // namespace msg
