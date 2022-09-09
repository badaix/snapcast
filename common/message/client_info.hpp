/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef MESSAGE_CLIENT_INFO_HPP
#define MESSAGE_CLIENT_INFO_HPP

// local headers
#include "json_message.hpp"


namespace msg
{

/// Client information sent from client to server
/// Might also be used for sync stats and latency estimations
class ClientInfo : public JsonMessage
{
public:
    ClientInfo() : JsonMessage(message_type::kClientInfo)
    {
        setVolume(100);
        setMuted(false);
    }

    ~ClientInfo() override = default;

    uint16_t getVolume()
    {
        return get("volume", static_cast<uint16_t>(100));
    }

    bool isMuted()
    {
        return get("muted", false);
    }

    void setVolume(uint16_t volume)
    {
        msg["volume"] = volume;
    }

    void setMuted(bool muted)
    {
        msg["muted"] = muted;
    }
};
} // namespace msg


#endif
