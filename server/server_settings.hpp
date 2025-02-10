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
#include "common/snap_exception.hpp"
#include "common/utils/string_utils.hpp"

// standard headers
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

/// Server settings
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

    /// Authorization settings
    struct Authorization
    {
        /// c'tor
        Authorization() = default;

        /// c'tor
        Authorization(const std::vector<std::string>& conf_roles, const std::vector<std::string>& conf_users)
        {
            for (const auto& role : conf_roles)
                roles.emplace_back(std::make_shared<ServerSettings::Authorization::Role>(role));

            auto empty_role = std::make_shared<ServerSettings::Authorization::Role>();
            for (const auto& conf_user : conf_users)
            {
                users.emplace_back(conf_user);
                ServerSettings::Authorization::User& user = users.back();
                if (user.role_name.empty())
                {
                    user.role = empty_role;
                }
                else
                {
                    const auto& role_iter = std::find_if(roles.begin(), roles.end(), [&](const auto& role) { return role->role == user.role_name; });
                    if (role_iter != roles.end())
                        user.role = *role_iter;
                }
                if (user.role == nullptr)
                    throw SnapException("Role '" + user.role_name + "' for user '" + user.name + "' not found");
            }
        }

        /// Role settings
        struct Role
        {
            /// c'tor
            Role() = default;

            /// c'tor
            explicit Role(const std::string& role_permissions)
            {
                std::string perm;
                role = utils::string::split_left(role_permissions, ':', perm);
                permissions = utils::string::split(perm, ',');
            }

            /// role name
            std::string role;
            /// permissions
            std::vector<std::string> permissions;
        };

        /// User settings
        struct User
        {
            /// c'tor
            explicit User(const std::string& user_password_role)
            {
                std::string perm;
                name = utils::string::split_left(user_password_role, ':', perm);
                password = utils::string::split_right(perm, ':', role_name);
            }

            /// user name
            std::string name;
            /// password
            std::string password;
            /// role
            std::string role_name;
            /// role
            std::shared_ptr<Role> role;
        };

        /// is auth enabled
        bool enabled{false};
        /// users
        std::vector<User> users;
        /// roles
        std::vector<std::shared_ptr<Role>> roles;
    };

    /// HTTP settings
    struct Http
    {
        /// enable HTTP server
        bool enabled{true};
        /// enable HTTPS
        bool ssl_enabled{false};
        /// HTTP port
        size_t port{1780};
        /// HTTPS port
        size_t ssl_port{1788};
        /// HTTP listen address
        std::vector<std::string> bind_to_address{{"::"}};
        /// HTTPS listen address
        std::vector<std::string> ssl_bind_to_address{{"::"}};
        /// doc root directory
        std::string doc_root;
        /// HTTP server host name
        std::string host{"<hostname>"};
        /// URL prefix when serving album art
        std::string url_prefix;
    };

    /// TCP streaming client settings
    struct Tcp
    {
        /// enable plain TCP audio streaming
        bool enabled{true};
        /// TCP port
        size_t port{1705};
        /// TCP listen addresses
        std::vector<std::string> bind_to_address{{"::"}};
    };

    /// Stream settings
    struct Stream
    {
        /// Audio streaming port
        size_t port{1704};
        /// Directory for stream plugins
        std::filesystem::path plugin_dir{"/usr/share/snapserver/plug-ins"};
        /// Stream sources
        std::vector<std::string> sources;
        /// Default codec
        std::string codec{"flac"};
        /// Default end to end delay
        int32_t bufferMs{1000};
        /// Default sample format
        std::string sampleFormat{"48000:16:2"};
        /// Default read size for stream sources
        size_t streamChunkMs{20};
        /// Send audio to muted clients?
        bool sendAudioToMutedClients{false};
        /// Liste addresses
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
        /// log sink
        std::string sink;
        /// log filter
        std::string filter{"*:info"};
    };

    Server server;                   ///< Server settings
    Ssl ssl;                         ///< SSL settings
    Authorization auth;              ///< Auth settings
    Http http;                       ///< HTTP settings
    Tcp tcp;                         ///< TCP settings
    Stream stream;                   ///< Stream settings
    StreamingClient streamingclient; ///< Client settings
    Logging logging;                 ///< Logging settings
};
