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

#ifndef CLIENT_SETTINGS_HPP
#define CLIENT_SETTINGS_HPP

#include <string>
#include <vector>

#include "common/sample_format.hpp"
#include "player/pcm_device.hpp"


struct ClientSettings
{
    enum class SharingMode
    {
        unspecified,
        exclusive,
        shared
    };

    struct Mixer
    {
        enum class Mode
        {
            hardware,
            software,
            script,
            none
        };

        Mode mode{Mode::software};
        std::string parameter{""};
    };

    struct Server
    {
        std::string host{""};
        size_t port{1704};
    };

    struct Player
    {
        std::string player_name{""};
        std::string parameter{""};
        int latency{0};
        player::PcmDevice pcm_device;
        SampleFormat sample_format;
        SharingMode sharing_mode{SharingMode::unspecified};
        Mixer mixer;
    };

    struct Logging
    {
        std::string sink{""};
        std::string filter{"*:info"};
    };

    size_t instance{1};
    std::string host_id;

    Server server;
    Player player;
    Logging logging;
};

#endif
