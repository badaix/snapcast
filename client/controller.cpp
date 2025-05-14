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
#include "chrony_client.hpp"

// standard headers
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

using namespace std;
using namespace player;

static constexpr auto LOG_TAG = "Controller";
static constexpr auto TIME_SYNC_INTERVAL = 1s;

Controller::Controller(boost::asio::io_context& io_context, const ClientSettings& settings)
    : io_context_(io_context),
#ifdef HAS_OPENSSL
      ssl_context_(boost::asio::ssl::context::tlsv12_client),
#endif
      timer_(io_context), settings_(settings), stream_(nullptr), decoder_(nullptr), player_(nullptr), serverSettings_(nullptr)
{
    // Initialize time provider
    // Protocol version defaults to V2
    TimeProvider::getInstance().configure(settings.time_sync);
    
    LOG(INFO, LOG_TAG) << "Initialized TimeProvider with preferred source: " 
                       << time_sync::timeSourceToString(time_sync::intToTimeSource(settings.time_sync.preferred_source))
                       << ", mode: " << time_sync::syncModeToString(settings.time_sync.mode) << "\n";
    
#ifdef HAS_OPENSSL
    if (settings.server.isSsl())
    {
        boost::system::error_code ec;
        if (settings.server.server_certificate.has_value())
        {
            LOG(DEBUG, LOG_TAG) << "Loading server certificate\n";
            ssl_context_.set_default_verify_paths(ec);
            if (ec.failed())
                LOG(WARNING, LOG_TAG) << "Failed to load system certificates: " << ec << "\n";
            if (!settings.server.server_certificate->empty())
            {
                ssl_context_.load_verify_file(settings.server.server_certificate.value().string(), ec);
                if (ec.failed())
                    throw SnapException("Failed to load server certificate: " + settings.server.server_certificate.value().string() + ": " + ec.message());
            }
        }

        if (!settings.server.certificate.empty() && !settings.server.certificate_key.empty())
        {
            if (!settings.server.key_password.empty())
            {
                ssl_context_.set_password_callback(
                    [pw = settings.server.key_password](size_t max_length, boost::asio::ssl::context_base::password_purpose purpose) -> string
                {
                    LOG(DEBUG, LOG_TAG) << "getPassword, purpose: " << purpose << ", max length: " << max_length << "\n";
                    return pw;
                });
            }
            LOG(DEBUG, LOG_TAG) << "Loading certificate file: " << settings.server.certificate << "\n";
            ssl_context_.use_certificate_chain_file(settings.server.certificate.string(), ec);
            if (ec.failed())
                throw SnapException("Failed to load certificate: " + settings.server.certificate.string() + ": " + ec.message());
            LOG(DEBUG, LOG_TAG) << "Loading certificate key file: " << settings.server.certificate_key << "\n";
            ssl_context_.use_private_key_file(settings.server.certificate_key.string(), boost::asio::ssl::context::pem, ec);
            if (ec.failed())
                throw SnapException("Failed to load private key file: " + settings.server.certificate_key.string() + ": " + ec.message());
        }
    }
#endif // HAS_OPENSSL
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
    clientConnection_->getNextMessage([this](const boost::system::error_code& ec, std::unique_ptr<msg::BaseMessage> response)
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
                player_->setVolume({serverSettings_->getVolume() / 100., serverSettings_->isMuted()});
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

            player_->setVolumeCallback([this](const Player::Volume& volume)
            {
                // Cache the last volume and check if it really changed in the player's volume callback
                static Player::Volume last_volume{-1, true};
                if (volume != last_volume)
                {
                    last_volume = volume;
                    auto info = std::make_shared<msg::ClientInfo>();
                    info->setVolume(static_cast<uint16_t>(volume.volume * 100.));
                    info->setMuted(volume.mute);
                    clientConnection_->send(info, [this](const boost::system::error_code& ec)
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
            player_->setVolume({serverSettings_->getVolume() / 100., serverSettings_->isMuted()});
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
    
    // Use the standardized helper to populate the time message
    auto& timeProvider = TimeProvider::getInstance();
    // Use protocol version V2 by default
    time_sync::ProtocolVersion protocol_version = time_sync::ProtocolVersion::V2;
    
    if (protocol_version != time_sync::ProtocolVersion::V1) {
        // For V2+ protocol, include time source information
        time_sync::TimeSyncInfo syncInfo = timeProvider.getSyncInfo();
        
        // Use the standardized helper to populate the time message
        time_sync::populateTimeMessage(timeReq.get(), syncInfo.source, protocol_version);
        
        LOG(DEBUG, LOG_TAG) << "Sending time sync message with protocol V" 
                            << static_cast<int>(timeReq->version) 
                            << ", source: " << time_sync::timeSourceToString(static_cast<time_sync::TimeSyncSource>(timeReq->source)) 
                            << ", quality: " << timeReq->quality << "\n";
    } else {
        // For V1 protocol, only send latency information
        timeReq->version = static_cast<uint8_t>(time_sync::ProtocolVersion::V1);
        LOG(DEBUG, LOG_TAG) << "Sending time sync message with legacy protocol V1\n";
    }
    
    clientConnection_->sendRequest<msg::Time>(timeReq, 2s,
                                              [this, quick_syncs](const boost::system::error_code& ec, const std::unique_ptr<msg::Time>& response) mutable
    {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Time sync request failed: " << ec.message() << "\n";
            reconnect();
            return;
        }
        
        auto& timeProvider = TimeProvider::getInstance();
        
        // With chrony-based synchronization, we don't need to calculate time differences
        // The system clocks are already synchronized by chrony
        
        try {
            // For logging purposes only - no actual time adjustment needed
            double diff_ms = 0;
            if (response->received > response->sent) {
                // Calculate difference in microseconds directly from tv struct
                tv diff = response->received - response->sent;
                diff_ms = (diff.sec * 1000000 + diff.usec) / 1000.0;
            }
            
            // Check if we're using the monotonic clock (local server)
            auto syncInfo = timeProvider.getSyncInfo();
            bool using_monotonic = (syncInfo.source == time_sync::TimeSyncSource::MONOTONIC);
            
            // Declare status variable outside the if/else blocks
            time_sync::TimeStatus status;
            
            if (using_monotonic) {
                // Skip the standard time synchronization process when using monotonic clock
                LOG(DEBUG, LOG_TAG) << "Using local monotonic clock, skipping time synchronization processing";
                
                // Create a simple status for logging purposes only
                status.active_source = time_sync::TimeSyncSource::MONOTONIC;
                status.active_source_info = syncInfo;
                status.protocol_version = static_cast<time_sync::ProtocolVersion>(response->version);
            } else {
                // Process the time response using the standardized helper
                status = time_sync::processTimeResponse(response.get(), diff_ms);
            }
            
            // Update protocol version if changed - common code for both branches
            if (status.protocol_version != timeProvider.getProtocolVersion()) {
                timeProvider.setProtocolVersion(status.protocol_version);
                LOG(INFO, LOG_TAG) << "Detected time sync protocol version: " 
                                  << static_cast<int>(status.protocol_version) << "\n";
            }
            
            // For V2+ protocol, handle time source from server
            if (status.protocol_version > time_sync::ProtocolVersion::V1) {
                LOG(DEBUG, LOG_TAG) << "Server time source: " 
                                   << time_sync::timeSourceToString(status.active_source) 
                                   << ", quality: " << status.active_source_info.quality << "\n";
                
                // Only log time source once during initialization
                static bool time_source_logged = false;
                
                // Simple check: if --on-server flag is set, skip chrony completely
                if (settings_.time_sync.on_server) {
                    // Already using monotonic clock, don't initialize chrony
                    if (!time_source_logged) {
                        LOG(WARNING, LOG_TAG) << "Using MONOTONIC time source (local server)";
                        LOG(INFO, LOG_TAG) << "Using local monotonic clock for time synchronization";
                        time_source_logged = true;
                    }
                } else if (status.active_source == time_sync::TimeSyncSource::CHRONY && !timeProvider.isLocalServer()) {
                    // Only initialize chrony for remote servers
                    std::string server_address = settings_.server.host;
                    initChronyClient(server_address);
                }
            } else {
                // For V1 protocol, just log that we're using a legacy protocol
                // With chrony-based synchronization, we don't need special handling
                LOG(INFO, LOG_TAG) << "Using legacy time sync protocol V1 - relying on chrony or system clock\n";
            }
        } catch (const std::exception& e) {
            LOG(ERROR, LOG_TAG) << "Error processing time response: " << e.what() << "\n";
        }
        
        // Determine next sync interval
        std::chrono::microseconds next;
        if (settings_.time_sync.sync_interval > 0) {
            next = std::chrono::microseconds(settings_.time_sync.sync_interval * 1000);
        } else {
            next = TIME_SYNC_INTERVAL; // Default interval
        }
        
        if (quick_syncs > 0)
        {
            if (--quick_syncs == 0) {
                auto syncInfo = timeProvider.getSyncInfo();
                LOG(INFO, LOG_TAG) << "Time synchronization complete, using time source: " 
                                   << time_sync::timeSourceToString(syncInfo.source) << "\n";
            }
            next = 100us;
        }
        
        timer_.expires_after(next);
        timer_.async_wait([this, quick_syncs](const boost::system::error_code& ec)
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
        LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << "\n";
    }

    timer_.expires_after(500ms);
    timer_.async_wait([this, handler](const boost::system::error_code& ec)
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
        browseMdns([this](const boost::system::error_code& ec, const std::string& host, uint16_t port)
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
                clientConnection_ = make_unique<ClientConnectionTcp>(io_context_, settings_.server);
                worker();
            }
        });
    }
    else
    {
        if (settings_.server.protocol == "ws")
            clientConnection_ = make_unique<ClientConnectionWs>(io_context_, settings_.server);
#ifdef HAS_OPENSSL
        else if (settings_.server.protocol == "wss")
            clientConnection_ = make_unique<ClientConnectionWss>(io_context_, ssl_context_, settings_.server);
#endif
        else
            clientConnection_ = make_unique<ClientConnectionTcp>(io_context_, settings_.server);
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
    timer_.async_wait([this](const boost::system::error_code& ec)
    {
        if (!ec)
        {
            worker();
        }
    });
}

void Controller::worker()
{
    clientConnection_->connect([this](const boost::system::error_code& ec)
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
                hello, 2s, [this](const boost::system::error_code& ec, std::unique_ptr<msg::ServerSettings> response) mutable
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
            
            // Log time synchronization information
            try {
                auto& timeProvider = TimeProvider::getInstance();
                time_sync::TimeSyncInfo syncInfo = timeProvider.getSyncInfo();
                
                // With chrony-based synchronization, we don't need to calculate time differences
                // The system clocks are already synchronized by chrony
                double diff_ms = 0;
                
                // Initialize and log time synchronization using the standardized function
                // Pass client-specific parameters: diff_ms, protocol version, and preferred source
                time_sync::initAndLogTimeSync(
                    LOG_TAG,
                    diff_ms,
                    timeProvider.getProtocolVersion(),
                    syncInfo.source);
            } catch (const std::exception& e) {
                LOG(WARNING, LOG_TAG) << "Failed to log time synchronization info: " << e.what();
            }
            
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

void Controller::initChronyClient(const std::string& server_address)
{
    static bool initialized = false;
    
    // Don't initialize twice
    if (initialized) {
        return;
    }
    
    // Simple check: if --on-server flag is set, skip chrony completely
    if (settings_.time_sync.on_server) {
        LOG(NOTICE, LOG_TAG) << "Skipping chrony setup as --on-server flag is set";
        LOG(INFO, LOG_TAG) << "Time synchronization complete, using time source: Monotonic";
        initialized = true;
        return; // Exit early - don't even attempt chrony initialization
    }
    
    // We need to use chrony for remote servers
    LOG(INFO, LOG_TAG) << "Initializing chrony client for time synchronization with server: " << server_address;
    
    // Create a temporary directory for chrony configuration
    std::string config_dir = "/tmp/snapclient_chrony_" + std::to_string(getpid());
    
    // Initialize chrony client
    auto& chrony_client = snapclient::ChronyClient::getInstance();
    try {
        // Initialize chrony client - will throw if chrony is not available
        chrony_client.init(config_dir);
        
        // Connect to the server's chrony master - will throw if connection fails
        chrony_client.connectToServer(server_address);
        
        LOG(NOTICE, LOG_TAG) << "Connected to chrony server at " << server_address;
        
        // Verify synchronization is working
        chrony_client.checkSynchronization();
        
        // Only log this if we're actually using chrony
        auto& timeProvider = TimeProvider::getInstance();
        if (timeProvider.getSyncInfo().source == time_sync::TimeSyncSource::CHRONY) {
            LOG(INFO, LOG_TAG) << "Using chrony for time synchronization";
        }
        
        initialized = true;
    } catch (const std::exception& e) {
        LOG(ERROR, LOG_TAG) << "Chrony initialization failed: " << e.what();
        // Don't throw, just log the error and continue with system time
        LOG(WARNING, LOG_TAG) << "Falling back to system time";
        LOG(INFO, LOG_TAG) << "Time synchronization complete, using time source: System";
        initialized = true;
    }
}

// Chrony connections are maintained throughout the client's lifetime
// No disconnection method needed
