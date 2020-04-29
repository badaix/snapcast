/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#ifndef CLIENT_SETTINGS_H
#define CLIENT_SETTINGS_H

#include "json_message.hpp"


namespace msg
{

class ClientSettings : public JsonMessage
{
public:
    ClientSettings() : JsonMessage(message_type::kClientSettings)
    {
        setVolume(100);
        setMuted(false);
    }

    ~ClientSettings() override = default;

    uint16_t getVolume()
    {
        return get("volume", 100);
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
}


#endif
