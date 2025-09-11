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
#include "common/sample_format.hpp"
#include "player/pcm_device.hpp"

// standard headers
#include <filesystem>
#include <optional>
#include <string>


/// Snapclient settings
struct ClientSettings
{
    /// Sharing mode for audio device
    enum class SharingMode
    {
        unspecified, ///< unspecified
        exclusive,   ///< exclusice access
        shared       ///< shared access
    };

    /// Mixer settings
    struct Mixer
    {
        /// Mixer mode
        enum class Mode
        {
            hardware, ///< hardware mixer
            software, ///< software mixer
            script,   ///< run a mixer script
            none      ///< no mixer
        };

        /// the configured mixer mode
        Mode mode{Mode::software};
        /// mixer parameter
        std::string parameter;
    };

    /// Server settings
    struct Server
    {
        /// Auth info
        struct Auth
        {
            /// the scheme (Basic, Plain, bearer, ...)
            std::string scheme;
            /// the param (base64 encoded "<user>:<password>", "<user>:<password>", token, ...)
            std::string param;
        };

        /// server host or IP address
        std::string host;
        /// protocol: "tcp", "ws" or "wss"
        std::string protocol{"tcp"};
        /// server port
        size_t port{1704};
        /// auth info
        std::optional<Auth> auth;
        /// server certificate
        std::optional<std::filesystem::path> server_certificate;
        /// Certificate file
        std::filesystem::path certificate;
        /// Private key file
        std::filesystem::path certificate_key;
        /// Password for encrypted key file
        std::string key_password;
        /// Is ssl in use?
        bool isSsl() const
        {
            return (protocol == "wss");
        }
    };

    /// The audio player (DAC)
    struct Player
    {
        /// name of the player
        std::string player_name;
        /// player parameters
        std::string parameter;
        /// additional latency of the DAC [ms]
        int latency{0};
        /// additional latency for RIST transport [ms] - added to buffer tolerance
        int rist_latency{0};
        /// the DAC
        player::PcmDevice pcm_device;
        /// Sampleformat to be uses, i.e. 48000:16:2
        SampleFormat sample_format;
        /// The sharing mode
        SharingMode sharing_mode{SharingMode::unspecified};
        /// Mixer settings
        Mixer mixer;
    };

    /// Log settings
    struct Logging
    {
        /// The log sink (null,system,stdout,stderr,file)
        std::string sink;
        /// Log filter
        std::string filter{"*:info"};
    };

    /// The snapclient process instance
    size_t instance{1};
    /// The host id, presented to the server
    std::string host_id;

    /// Server settings
    Server server;
    /// Player settings
    Player player;
    /// Logging settings
    Logging logging;
};
