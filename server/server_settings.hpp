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
#include "common/utils/string_utils.hpp"

// standard headers
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>


struct ServerSettings
{
    struct Server
    {
        int threads{-1};
        std::string pid_file{"/var/run/snapserver/pid"};
        std::string user{"snapserver"};
        std::string group;
        std::string data_dir;
    };

    struct Ssl
    {
        std::filesystem::path certificate;
        std::filesystem::path certificate_key;
        std::string key_password;

        bool enabled() const
        {
            return !certificate.empty() && !certificate_key.empty();
        }
    };

    struct User
    {
        explicit User(const std::string& user_permissions_password)
        {
            std::string perm;
            name = utils::string::split_left(user_permissions_password, ':', perm);
            perm = utils::string::split_left(perm, ':', password);
            permissions = utils::string::split(perm, ',');
        }

        std::string name;
        std::vector<std::string> permissions;
        std::string password;
    };

    std::vector<User> users;

    struct Http
    {
        bool enabled{true};
        bool ssl_enabled{false};
        size_t port{1780};
        size_t ssl_port{1788};
        std::vector<std::string> bind_to_address{{"::"}};
        std::vector<std::string> ssl_bind_to_address{{"::"}};
        std::string doc_root;
        std::string host{"<hostname>"};
        std::string url_prefix;
    };

    struct Tcp
    {
        bool enabled{true};
        size_t port{1705};
        std::vector<std::string> bind_to_address{{"::"}};
    };

    struct Stream
    {
        size_t port{1704};
        std::filesystem::path plugin_dir{"/usr/share/snapserver/plug-ins"};
        std::vector<std::string> sources;
        std::string codec{"flac"};
        int32_t bufferMs{1000};
        std::string sampleFormat{"48000:16:2"};
        size_t streamChunkMs{20};
        bool sendAudioToMutedClients{false};
        std::vector<std::string> bind_to_address{{"::"}};
    };

    struct StreamingClient
    {
        uint16_t initialVolume{100};
    };

    struct Logging
    {
        std::string sink;
        std::string filter{"*:info"};
    };

    Server server;
    Ssl ssl;
    Http http;
    Tcp tcp;
    Stream stream;
    StreamingClient streamingclient;
    Logging logging;
};
