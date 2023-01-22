/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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

#ifndef CONTROLLER_HPP
#define CONTROLLER_HPP

// local headers
#include "client_connection.hpp"
#include "client_settings.hpp"
#include "common/message/message.hpp"
#include "common/message/server_settings.hpp"
#include "decoder/decoder.hpp"
#include "player/player.hpp"
#include "stream.hpp"

// 3rd party headers

// standard headers
#include <atomic>
#include <thread>

using namespace std::chrono_literals;

/// Forwards PCM data to the audio player
/**
 * Sets up a connection to the server (using ClientConnection)
 * Sets up the audio decoder and player.
 * Decodes audio (message_type::kWireChunk) and feeds PCM to the audio stream buffer
 * Does timesync with the server
 */
class Controller
{
public:
    Controller(boost::asio::io_context& io_context, const ClientSettings& settings); //, std::unique_ptr<MetadataAdapter> meta);
    void start();
    // void stop();
    static std::vector<std::string> getSupportedPlayerNames();

private:
    using MdnsHandler = std::function<void(const boost::system::error_code& ec, const std::string& host, uint16_t port)>;
    void worker();
    void reconnect();
    void browseMdns(const MdnsHandler& handler);

    template <typename PlayerType>
    std::unique_ptr<player::Player> createPlayer(ClientSettings::Player& settings, const std::string& player_name);

    void getNextMessage();
    void sendTimeSyncMessage(int quick_syncs);

    boost::asio::io_context& io_context_;
    boost::asio::steady_timer timer_;
    ClientSettings settings_;
    SampleFormat sampleFormat_;
    std::unique_ptr<ClientConnection> clientConnection_;
    std::shared_ptr<Stream> stream_;
    std::unique_ptr<decoder::Decoder> decoder_;
    std::unique_ptr<player::Player> player_;
    // std::unique_ptr<MetadataAdapter> meta_;
    std::unique_ptr<msg::ServerSettings> serverSettings_;
    std::unique_ptr<msg::CodecHeader> headerChunk_;
};


#endif
