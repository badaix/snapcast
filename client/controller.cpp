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

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

// prototype/interface header file
#include "controller.hpp"

// local headers
#include "decoder/null_decoder.hpp"
#include "decoder/pcm_decoder.hpp"
#if defined(HAS_OGG) && (defined(HAS_TREMOR) || defined(HAS_VORBIS))
#include "decoder/ogg_decoder.hpp"
#endif
#if defined(HAS_FLAC)
#include "decoder/flac_decoder.hpp"
#endif
#if defined(HAS_OPUS)
#include "decoder/opus_decoder.hpp"
#endif

#ifdef HAS_ALSA
#include "player/alsa_player.hpp"
#endif
#ifdef HAS_PULSE
#include "player/pulse_player.hpp"
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
#ifdef HAS_WASAPI
#include "player/wasapi_player.hpp"
#endif
#include "player/file_player.hpp"

#include "browseZeroConf/browse_mdns.hpp"
#include "common/aixlog.hpp"
#include "common/message/client_info.hpp"
#include "common/message/hello.hpp"
#include "common/message/time.hpp"
#include "common/snap_exception.hpp"
#include "time_provider.hpp"

// standard headers
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

using namespace std;
using namespace player;

static constexpr auto LOG_TAG = "Controller";
static constexpr auto TIME_SYNC_INTERVAL = 1s;

Controller::Controller(boost::asio::io_context& io_context, const ClientSettings& settings) //, std::unique_ptr<MetadataAdapter> meta)
    : io_context_(io_context), timer_(io_context), settings_(settings), stream_(nullptr), decoder_(nullptr), player_(nullptr),
      serverSettings_(nullptr) // meta_(std::move(meta)),
{
}


template <typename PlayerType>
std::unique_ptr<Player> Controller::createPlayer(ClientSettings::Player& settings, const std::string& player_name)
{
    if (settings.player_name.empty() || settings.player_name == player_name)
    {
        settings.player_name = player_name;
        return make_unique<PlayerType>(io_context_, settings, stream_);
    }
    return nullptr;
}

std::vector<std::string> Controller::getSupportedPlayerNames()
{
    std::vector<std::string> result;
#ifdef HAS_ALSA
    result.emplace_back(player::ALSA);
#endif
#ifdef HAS_PULSE
    result.emplace_back(player::PULSE);
#endif
#ifdef HAS_OBOE
    result.emplace_back(player::OBOE);
#endif
#ifdef HAS_OPENSL
    result.emplace_back(player::OPENSL);
#endif
#ifdef HAS_COREAUDIO
    result.emplace_back(player::COREAUDIO);
#endif
#ifdef HAS_WASAPI
    result.emplace_back(player::WASAPI);
#endif
    result.emplace_back(player::FILE);
    return result;
}


void Controller::getNextMessage()
{
    clientConnection_->getNextMessage(
        [this](const boost::system::error_code& ec, std::unique_ptr<msg::BaseMessage> response)
        {
        if (ec)
        {
            reconnect();
            return;
        }

        if (!response)
        {
            return getNextMessage();
        }

        if (response->type == message_type::kWireChunk)
        {
            if (stream_ && decoder_)
            {
                // execute on the io_context to do the (costly) decoding on another thread (if more than one thread is used)
                // boost::asio::post(io_context_, [this, response = std::move(response)]() mutable {
                auto pcmChunk = msg::message_cast<msg::PcmChunk>(std::move(response));
                pcmChunk->format = sampleFormat_;
                // LOG(TRACE, LOG_TAG) << "chunk: " << pcmChunk->payloadSize << ", sampleFormat: " << sampleFormat_.toString() << "\n";
                if (decoder_->decode(pcmChunk.get()))
                {
                    // LOG(TRACE, LOG_TAG) << ", decoded: " << pcmChunk->payloadSize << ", Duration: " << pcmChunk->durationMs() << ", sec: " <<
                    // pcmChunk->timestamp.sec << ", usec: " << pcmChunk->timestamp.usec / 1000 << ", type: " << pcmChunk->type << "\n";
                    stream_->addChunk(std::move(pcmChunk));
                }
                // });
            }
        }
        else if (response->type == message_type::kServerSettings)
        {
            serverSettings_ = msg::message_cast<msg::ServerSettings>(std::move(response));
            LOG(INFO, LOG_TAG) << "ServerSettings - buffer: " << serverSettings_->getBufferMs() << ", latency: " << serverSettings_->getLatency()
                               << ", volume: " << serverSettings_->getVolume() << ", muted: " << serverSettings_->isMuted() << "\n";
            if (stream_ && player_)
            {
                player_->setVolume(serverSettings_->getVolume() / 100., serverSettings_->isMuted());
                stream_->setBufferLen(std::max(0, serverSettings_->getBufferMs() - serverSettings_->getLatency() - settings_.player.latency));
            }
        }
        else if (response->type == message_type::kCodecHeader)
        {
            headerChunk_ = msg::message_cast<msg::CodecHeader>(std::move(response));
            decoder_.reset(nullptr);
            stream_ = nullptr;
            player_.reset(nullptr);

            if (headerChunk_->codec == "pcm")
                decoder_ = make_unique<decoder::PcmDecoder>();
#if defined(HAS_OGG) && (defined(HAS_TREMOR) || defined(HAS_VORBIS))
            else if (headerChunk_->codec == "ogg")
                decoder_ = make_unique<decoder::OggDecoder>();
#endif
#if defined(HAS_FLAC)
            else if (headerChunk_->codec == "flac")
                decoder_ = make_unique<decoder::FlacDecoder>();
#endif
#if defined(HAS_OPUS)
            else if (headerChunk_->codec == "opus")
                decoder_ = make_unique<decoder::OpusDecoder>();
#endif
            else if (headerChunk_->codec == "null")
                decoder_ = make_unique<decoder::NullDecoder>();
            else
                throw SnapException("codec not supported: \"" + headerChunk_->codec + "\"");

            sampleFormat_ = decoder_->setHeader(headerChunk_.get());
            LOG(INFO, LOG_TAG) << "Codec: " << headerChunk_->codec << ", sampleformat: " << sampleFormat_.toString() << "\n";

            stream_ = make_shared<Stream>(sampleFormat_, settings_.player.sample_format);
            stream_->setBufferLen(std::max(0, serverSettings_->getBufferMs() - serverSettings_->getLatency() - settings_.player.latency));

#ifdef HAS_ALSA
            if (!player_)
                player_ = createPlayer<AlsaPlayer>(settings_.player, player::ALSA);
#endif
#ifdef HAS_PULSE
            if (!player_)
                player_ = createPlayer<PulsePlayer>(settings_.player, player::PULSE);
#endif
#ifdef HAS_OBOE
            if (!player_)
                player_ = createPlayer<OboePlayer>(settings_.player, player::OBOE);
#endif
#ifdef HAS_OPENSL
            if (!player_)
                player_ = createPlayer<OpenslPlayer>(settings_.player, player::OPENSL);
#endif
#ifdef HAS_COREAUDIO
            if (!player_)
                player_ = createPlayer<CoreAudioPlayer>(settings_.player, player::COREAUDIO);
#endif
#ifdef HAS_WASAPI
            if (!player_)
                player_ = createPlayer<WASAPIPlayer>(settings_.player, player::WASAPI);
#endif
            if (!player_ && (settings_.player.player_name == player::FILE))
                player_ = createPlayer<FilePlayer>(settings_.player, player::FILE);

            if (!player_)
                throw SnapException("No audio player support" + (settings_.player.player_name.empty() ? "" : " for: " + settings_.player.player_name));

            player_->setVolumeCallback(
                [this](double volume, bool muted)
                {
                static double last_volume(-1);
                static bool last_muted(true);
                if ((volume != last_volume) || (last_muted != muted))
                {
                    last_volume = volume;
                    last_muted = muted;
                    auto info = std::make_shared<msg::ClientInfo>();
                    info->setVolume(static_cast<uint16_t>(volume * 100.));
                    info->setMuted(muted);
                    clientConnection_->send(info,
                                            [this](const boost::system::error_code& ec)
                                            {
                        if (ec)
                        {
                            LOG(ERROR, LOG_TAG) << "Failed to send client info, error: " << ec.message() << "\n";
                            reconnect();
                            return;
                        }
                    });
                }
            });
            player_->start();
            // Don't change the initial hardware mixer volume on the user's device.
            // The player class will send the device's volume to the server instead
            // if (settings_.player.mixer.mode != ClientSettings::Mixer::Mode::hardware)
            // {
            player_->setVolume(serverSettings_->getVolume() / 100., serverSettings_->isMuted());
            // }
        }
        else
        {
            LOG(WARNING, LOG_TAG) << "Unexpected message received, type: " << response->type << "\n";
        }
        getNextMessage();
    });
}


void Controller::sendTimeSyncMessage(int quick_syncs)
{
    auto timeReq = std::make_shared<msg::Time>();
    clientConnection_->sendRequest<msg::Time>(timeReq, 2s,
                                              [this, quick_syncs](const boost::system::error_code& ec, const std::unique_ptr<msg::Time>& response) mutable
                                              {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Time sync request failed: " << ec.message() << "\n";
            reconnect();
            return;
        }
        else
        {
            TimeProvider::getInstance().setDiff(response->latency, response->received - response->sent);
        }

        std::chrono::microseconds next = TIME_SYNC_INTERVAL;
        if (quick_syncs > 0)
        {
            if (--quick_syncs == 0)
                LOG(INFO, LOG_TAG) << "diff to server [ms]: "
                                   << static_cast<float>(TimeProvider::getInstance().getDiffToServer<chronos::usec>().count()) / 1000.f << "\n";
            next = 100us;
        }
        timer_.expires_after(next);
        timer_.async_wait(
            [this, quick_syncs](const boost::system::error_code& ec)
            {
            if (!ec)
            {
                sendTimeSyncMessage(quick_syncs);
            }
        });
    });
}

void Controller::browseMdns(const MdnsHandler& handler)
{
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
    try
    {
        BrowseZeroConf browser;
        mDNSResult avahiResult;
        if (browser.browse("_snapcast._tcp", avahiResult, 1000))
        {
            string host = avahiResult.ip;
            uint16_t port = avahiResult.port;
            if (avahiResult.ip_version == IPVersion::IPv6)
                host += "%" + cpt::to_string(avahiResult.iface_idx);
            handler({}, host, port);
            return;
        }
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << std::endl;
    }

    timer_.expires_after(500ms);
    timer_.async_wait(
        [this, handler](const boost::system::error_code& ec)
        {
        if (!ec)
        {
            browseMdns(handler);
        }
        else
        {
            handler(ec, "", 0);
        }
    });
#else
    handler(boost::asio::error::operation_not_supported, "", 0);
#endif
}

void Controller::start()
{
    if (settings_.server.host.empty())
    {
        browseMdns(
            [this](const boost::system::error_code& ec, const std::string& host, uint16_t port)
            {
            if (ec)
            {
                LOG(ERROR, LOG_TAG) << "Failed to browse MDNS, error: " << ec.message() << "\n";
            }
            else
            {
                settings_.server.host = host;
                settings_.server.port = port;
                LOG(INFO, LOG_TAG) << "Found server " << settings_.server.host << ":" << settings_.server.port << "\n";
                clientConnection_ = make_unique<ClientConnection>(io_context_, settings_.server);
                worker();
            }
        });
    }
    else
    {
        clientConnection_ = make_unique<ClientConnection>(io_context_, settings_.server);
        worker();
    }
}


// void Controller::stop()
// {
//     LOG(DEBUG, LOG_TAG) << "Stopping\n";
//     timer_.cancel();
// }

void Controller::reconnect()
{
    timer_.cancel();
    clientConnection_->disconnect();
    player_.reset();
    stream_.reset();
    decoder_.reset();
    timer_.expires_after(1s);
    timer_.async_wait(
        [this](const boost::system::error_code& ec)
        {
        if (!ec)
        {
            worker();
        }
    });
}

void Controller::worker()
{
    clientConnection_->connect(
        [this](const boost::system::error_code& ec)
        {
        if (!ec)
        {
            // LOG(INFO, LOG_TAG) << "Connected!\n";
            string macAddress = clientConnection_->getMacAddress();
            if (settings_.host_id.empty())
                settings_.host_id = ::getHostId(macAddress);

            // Say hello to the server
            auto hello = std::make_shared<msg::Hello>(macAddress, settings_.host_id, settings_.instance);
            clientConnection_->sendRequest<msg::ServerSettings>(
                hello, 2s,
                [this](const boost::system::error_code& ec, std::unique_ptr<msg::ServerSettings> response) mutable
                {
                if (ec)
                {
                    LOG(ERROR, LOG_TAG) << "Failed to send hello request, error: " << ec.message() << "\n";
                    reconnect();
                    return;
                }
                else
                {
                    serverSettings_ = std::move(response);
                    LOG(INFO, LOG_TAG) << "ServerSettings - buffer: " << serverSettings_->getBufferMs() << ", latency: " << serverSettings_->getLatency()
                                       << ", volume: " << serverSettings_->getVolume() << ", muted: " << serverSettings_->isMuted() << "\n";
                }
                });

            // Do initial time sync with the server
            sendTimeSyncMessage(50);
            // Start receiver loop
            getNextMessage();
        }
        else
        {
            LOG(ERROR, LOG_TAG) << "Error: " << ec.message() << "\n";
            reconnect();
        }
    });
}
