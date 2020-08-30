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

#ifndef SERVER_SETTINGS_HPP
#define SERVER_SETTINGS_HPP

#include <string>
#include <vector>

struct ServerSettings
{
    struct Server
    {
        int threads{-1};
        std::string pid_file{"/var/run/snapserver/pid"};
        std::string user{"snapserver"};
        std::string group{""};
        std::string data_dir{""};
    };

    struct Http
    {
        bool enabled{true};
        size_t port{1780};
        std::vector<std::string> bind_to_address{{"0.0.0.0"}};
        std::string doc_root{""};
    };

    struct Tcp
    {
        bool enabled{true};
        size_t port{1705};
        std::vector<std::string> bind_to_address{{"0.0.0.0"}};
    };

    struct Stream
    {
        size_t port{1704};
        std::vector<std::string> sources;
        std::string codec{"flac"};
        int32_t bufferMs{1000};
        std::string sampleFormat{"48000:16:2"};
        size_t streamChunkMs{20};
        bool sendAudioToMutedClients{false};
        std::vector<std::string> bind_to_address{{"0.0.0.0"}};
    };

    struct Logging
    {
        std::string sink{""};
        std::string filter{"*:info"};
    };

    Server server;
    Http http;
    Tcp tcp;
    Stream stream;
    Logging logging;
};

#endif
