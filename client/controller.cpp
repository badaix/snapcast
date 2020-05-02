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

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include "controller.hpp"
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
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "message/hello.hpp"
#include "message/time.hpp"
#include "time_provider.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

using namespace std;


Controller::Controller(const ClientSettings& settings, std::unique_ptr<MetadataAdapter> meta)
    : MessageReceiver(), settings_(settings), active_(false), stream_(nullptr), decoder_(nullptr), player_(nullptr), meta_(std::move(meta)),
      serverSettings_(nullptr), async_exception_(nullptr)
{
}


void Controller::onException(ClientConnection* /*connection*/, std::exception_ptr exception)
{
    LOG(ERROR) << "Controller::onException\n";
    async_exception_ = exception;
}


void Controller::onMessageReceived(ClientConnection* /*connection*/, const msg::BaseMessage& baseMessage, char* buffer)
{
    std::lock_guard<std::mutex> lock(receiveMutex_);
    if (baseMessage.type == message_type::kWireChunk)
    {
        if (stream_ && decoder_)
        {
            auto pcmChunk = make_unique<msg::PcmChunk>(sampleFormat_, 0);
            pcmChunk->deserialize(baseMessage, buffer);
            // LOG(DEBUG) << "chunk: " << pcmChunk->payloadSize << ", sampleFormat: " << sampleFormat_.getFormat() << "\n";
            if (decoder_->decode(pcmChunk.get()))
            {
                // TODO: do decoding in thread?
                stream_->addChunk(move(pcmChunk));
                // LOG(DEBUG) << ", decoded: " << pcmChunk->payloadSize << ", Duration: " << pcmChunk->getDuration() << ", sec: " << pcmChunk->timestamp.sec <<
                // ", usec: " << pcmChunk->timestamp.usec/1000 << ", type: " << pcmChunk->type << "\n";
            }
        }
    }
    else if (baseMessage.type == message_type::kTime)
    {
        msg::Time reply;
        reply.deserialize(baseMessage, buffer);
        TimeProvider::getInstance().setDiff(reply.latency, reply.received - reply.sent); // ToServer(diff / 2);
    }
    else if (baseMessage.type == message_type::kServerSettings)
    {
        serverSettings_ = make_unique<msg::ServerSettings>();
        serverSettings_->deserialize(baseMessage, buffer);
        LOG(INFO) << "ServerSettings - buffer: " << serverSettings_->getBufferMs() << ", latency: " << serverSettings_->getLatency()
                  << ", volume: " << serverSettings_->getVolume() << ", muted: " << serverSettings_->isMuted() << "\n";
        if (stream_ && player_)
        {
            player_->setVolume(serverSettings_->getVolume() / 100.);
            player_->setMute(serverSettings_->isMuted());
            stream_->setBufferLen(std::max(0, serverSettings_->getBufferMs() - serverSettings_->getLatency() - settings_.player.latency));
        }
    }
    else if (baseMessage.type == message_type::kCodecHeader)
    {
        headerChunk_ = make_unique<msg::CodecHeader>();
        headerChunk_->deserialize(baseMessage, buffer);

        LOG(INFO) << "Codec: " << headerChunk_->codec << "\n";
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
        else
            throw SnapException("codec not supported: \"" + headerChunk_->codec + "\"");

        sampleFormat_ = decoder_->setHeader(headerChunk_.get());
        LOG(NOTICE) << TAG("state") << "sampleformat: " << sampleFormat_.getFormat() << "\n";

        stream_ = make_shared<Stream>(sampleFormat_, settings_.player.sample_format);
        stream_->setBufferLen(std::max(0, serverSettings_->getBufferMs() - serverSettings_->getLatency() - settings_.player.latency));

        const auto& pcm_device = settings_.player.pcm_device;
        const auto& player_name = settings_.player.player_name;
        player_ = nullptr;
#ifdef HAS_ALSA
        if (!player_ && (player_name.empty() || (player_name == "alsa")))
            player_ = make_unique<AlsaPlayer>(pcm_device, stream_);
#endif
#ifdef HAS_OBOE
        if (!player_ && (player_name.empty() || (player_name == "oboe")))
            player_ = make_unique<OboePlayer>(pcm_device, stream_);
#endif
#ifdef HAS_OPENSL
        if (!player_ && (player_name.empty() || (player_name == "opensl")))
            player_ = make_unique<OpenslPlayer>(pcm_device, stream_);
#endif
#ifdef HAS_COREAUDIO
        if (!player_ && (player_name.empty() || (player_name == "coreaudio")))
            player_ = make_unique<CoreAudioPlayer>(pcm_device, stream_);
#endif
#ifdef HAS_WASAPI
        if (!player_ && (player_name.empty() || (player_name == "wasapi")))
            player_ = make_unique<WASAPIPlayer>(pcm_device, stream_, settings_.player.sharing_mode);
#endif
        if (!player_)
            throw SnapException("No audio player support");
        player_->setVolume(serverSettings_->getVolume() / 100.);
        player_->setMute(serverSettings_->isMuted());
        player_->start();
    }
    else if (baseMessage.type == message_type::kStreamTags)
    {
        if (meta_)
        {
            msg::StreamTags streamTags_;
            streamTags_.deserialize(baseMessage, buffer);
            meta_->push(streamTags_.msg);
        }
    }

    // if (baseMessage.type != message_type::kTime)
    //     if (sendTimeSyncMessage(1000))
    //         LOG(DEBUG) << "time sync onMessageReceived\n";
}


bool Controller::sendTimeSyncMessage(const std::chrono::milliseconds& after)
{
    static chronos::time_point_clk lastTimeSync(chronos::clk::now());
    auto now = chronos::clk::now();
    if (lastTimeSync + after > now)
        return false;

    lastTimeSync = now;
    msg::Time timeReq;
    clientConnection_->send(&timeReq);
    return true;
}


void Controller::start()
{
    clientConnection_ = make_unique<ClientConnection>(this, settings_.server.host, settings_.server.port);
    controllerThread_ = thread(&Controller::worker, this);
}


void Controller::run()
{
    clientConnection_ = make_unique<ClientConnection>(this, settings_.server.host, settings_.server.port);
    worker();
    // controllerThread_ = thread(&Controller::worker, this);
}


void Controller::stop()
{
    LOG(DEBUG) << "Stopping Controller" << endl;
    active_ = false;
    controllerThread_.join();
    clientConnection_->stop();
}


void Controller::worker()
{
    active_ = true;

    while (active_)
    {
        try
        {
            clientConnection_->start();

            string macAddress = clientConnection_->getMacAddress();
            if (settings_.host_id.empty())
                settings_.host_id = ::getHostId(macAddress);

            /// Say hello to the server
            msg::Hello hello(macAddress, settings_.host_id, settings_.instance);
            clientConnection_->send(&hello);

            /// Do initial time sync with the server
            msg::Time timeReq;
            for (size_t n = 0; n < 50 && active_; ++n)
            {
                if (async_exception_)
                {
                    LOG(DEBUG) << "Async exception\n";
                    std::rethrow_exception(async_exception_);
                }

                auto reply = clientConnection_->sendReq<msg::Time>(&timeReq, chronos::msec(2000));
                if (reply)
                {
                    TimeProvider::getInstance().setDiff(reply->latency, reply->received - reply->sent);
                    chronos::usleep(100);
                }
            }
            LOG(INFO) << "diff to server [ms]: " << (float)TimeProvider::getInstance().getDiffToServer<chronos::usec>().count() / 1000.f << "\n";

            /// Main loop
            while (active_)
            {
                if (async_exception_)
                {
                    LOG(DEBUG) << "Async exception\n";
                    std::rethrow_exception(async_exception_);
                }

                if (sendTimeSyncMessage(1000ms))
                    LOG(DEBUG) << "time sync main loop\n";
                this_thread::sleep_for(100ms);
            }
        }
        catch (const std::exception& e)
        {
            async_exception_ = nullptr;
            LOG(ERROR) << "Exception in Controller::worker(): " << e.what() << endl;
            clientConnection_->stop();
            player_.reset();
            stream_.reset();
            decoder_.reset();
            for (size_t n = 0; (n < 10) && active_; ++n)
                chronos::sleep(100);
        }
    }
    LOG(DEBUG) << "Thread stopped\n";
}
