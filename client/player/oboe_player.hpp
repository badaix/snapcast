/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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
#include "player.hpp"

// 3rd party headers
#include <oboe/LatencyTuner.h>
#include <oboe/Oboe.h>

// standard headers
#include <vector>


namespace player
{

static constexpr auto OBOE = "oboe";

/// Android Oboe Audio Player
/**
 * Player implementation for Android Oboe
 */
class OboePlayer : public Player, public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback
{
public:
    /// c'tor
    OboePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    /// d'tor
    virtual ~OboePlayer();

    void start() override;
    void stop() override;

protected:
    /// AudioStreamDataCallback::onAudioReady override
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* oboeStream, void* audioData, int32_t numFrames) override;

    /// AudioStreamErrorCallback::onErrorBeforeClose override
    void onErrorBeforeClose(oboe::AudioStream* oboeStream, oboe::Result error) override;
    /// AudioStreamErrorCallback::onErrorAfterClose override
    void onErrorAfterClose(oboe::AudioStream* oboeStream, oboe::Result error) override;

protected:
    /// Open the oboe stream
    oboe::Result openStream();
    /// Get current audio playout delay
    double getCurrentOutputLatencyMillis() const;

    bool needsThread() const override;
    /// The oboe output stream
    std::shared_ptr<oboe::AudioStream> out_stream_;
    /// Latency tuner to optimize the latency
    std::unique_ptr<oboe::LatencyTuner> latency_tuner_;

    /// Audio data buffer used for chaning the bit depth
    std::vector<char> audio_data_;
};

} // namespace player
