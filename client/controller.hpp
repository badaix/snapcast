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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "decoder/decoder.hpp"
#include "message/message.hpp"
#include "message/server_settings.hpp"
#include "message/stream_tags.hpp"
#include <atomic>
#include <thread>
#ifdef HAS_ALSA
#include "player/alsa_player.hpp"
#endif
#ifdef HAS_OPENSL
#include "player/opensl_player.hpp"
#endif
#ifdef HAS_OBOE
#include "player/oboe_player.hpp"
#endif
#ifdef HAS_COREAUDIO
#include "player/coreaudio_player.hpp"
#endif
#include "client_connection.hpp"
#include "client_settings.hpp"
#include "metadata.hpp"
#include "stream.hpp"

using namespace std::chrono_literals;

/// Forwards PCM data to the audio player
/**
 * Sets up a connection to the server (using ClientConnection)
 * Sets up the audio decoder and player.
 * Decodes audio (message_type::kWireChunk) and feeds PCM to the audio stream buffer
 * Does timesync with the server
 */
class Controller : public MessageReceiver
{
public:
    Controller(const ClientSettings& settings, std::unique_ptr<MetadataAdapter> meta);
    void start();
    void run();
    void stop();

    /// Implementation of MessageReceiver.
    /// ClientConnection passes messages from the server through these callbacks
    void onMessageReceived(ClientConnection* connection, const msg::BaseMessage& baseMessage, char* buffer) override;

    /// Implementation of MessageReceiver.
    /// Used for async exception reporting
    void onException(ClientConnection* connection, std::exception_ptr exception) override;

private:
    void worker();
    bool sendTimeSyncMessage(const std::chrono::milliseconds& after = 1000ms);
    ClientSettings settings_;
    std::string meta_callback_;
    std::atomic<bool> active_;
    std::thread controllerThread_;
    SampleFormat sampleFormat_;
    std::unique_ptr<ClientConnection> clientConnection_;
    std::shared_ptr<Stream> stream_;
    std::unique_ptr<decoder::Decoder> decoder_;
    std::unique_ptr<Player> player_;
    std::unique_ptr<MetadataAdapter> meta_;
    std::unique_ptr<msg::ServerSettings> serverSettings_;
    std::unique_ptr<msg::CodecHeader> headerChunk_;
    std::mutex receiveMutex_;

    std::exception_ptr async_exception_;
};


#endif
