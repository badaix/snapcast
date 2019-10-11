/***
    This file is part of snapcast
    Copyright (C) 2014-2019  Johannes Pohl

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
    struct HttpSettings
    {
        bool enabled{true};
        size_t port{1780};
        std::vector<std::string> bind_to_address{{"0.0.0.0"}};
        std::string doc_root{""};
    };

    struct TcpSettings
    {
        bool enabled{true};
        size_t port{1705};
        std::vector<std::string> bind_to_address{{"0.0.0.0"}};
    };

    struct StreamSettings
    {
        size_t port{1704};
        std::vector<std::string> pcmStreams;
        std::string codec{"flac"};
        int32_t bufferMs{1000};
        std::string sampleFormat{"48000:16:2"};
        size_t streamReadMs{20};
        bool sendAudioToMutedClients{false};
    };

    struct LoggingSettings
    {
        bool debug{false};
        std::string debug_logfile{""};
    };

    HttpSettings http;
    TcpSettings tcp;
    StreamSettings stream;
    LoggingSettings logging;
};

#endif
