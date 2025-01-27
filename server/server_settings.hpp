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
    /// Launch settings
    struct Server
    {
        /// Number of worker threads
        int threads{-1};
        /// PID file, if running as daemon
        std::string pid_file{"/var/run/snapserver/pid"};
        /// User when running as deaemon
        std::string user{"snapserver"};
        /// Group when running as deaemon
        std::string group;
        /// Server data dir
        std::string data_dir;
    };

    /// SSL settings
    struct Ssl
    {
        /// Certificate file
        std::filesystem::path certificate;
        /// Private key file
        std::filesystem::path certificate_key;
        /// Password for encrypted key file
        std::string key_password;
        /// Verify client certificates
        bool verify_clients = false;
        /// Client CA certificates
        std::vector<std::filesystem::path> client_certs;

        /// @return if SSL is enabled
        bool enabled() const
        {
            return !certificate.empty() && !certificate_key.empty();
        }
    };

    /// User settings
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


    /// HTTP settings
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

    /// TCP streaming client settings
    struct Tcp
    {
        bool enabled{true};
        size_t port{1705};
        std::vector<std::string> bind_to_address{{"::"}};
    };

    /// Stream settings
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

    /// Client settings
    struct StreamingClient
    {
        /// Initial volume of new clients
        uint16_t initialVolume{100};
    };

    /// Logging settings
    struct Logging
    {
        /// log sing
        std::string sink;
        /// log filter
        std::string filter{"*:info"};
    };

    Server server; ///< Server settings
    Ssl ssl; ///< SSL settings
    std::vector<User> users; ///< User settings
    Http http; ///< HTTP settings
    Tcp tcp; ///< TCP settings
    Stream stream; ///< Stream settings
    StreamingClient streamingclient; ///< Client settings
    Logging logging; ///< Logging settings
};
