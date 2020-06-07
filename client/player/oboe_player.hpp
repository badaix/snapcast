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

#ifndef OBOE_PLAYER_HPP
#define OBOE_PLAYER_HPP

#include <oboe/LatencyTuner.h>
#include <oboe/Oboe.h>

#include "player.hpp"

typedef int (*AndroidAudioCallback)(short* buffer, int num_samples);


/// OpenSL Audio Player
/**
 * Player implementation for Oboe
 */
class OboePlayer : public Player, public oboe::AudioStreamCallback
{
public:
    OboePlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    virtual ~OboePlayer();

    void start() override;
    void stop() override;

protected:
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* oboeStream, void* audioData, int32_t numFrames) override;
    double getCurrentOutputLatencyMillis() const;

    bool needsThread() const override;
    oboe::ManagedStream out_stream_;

    std::unique_ptr<oboe::LatencyTuner> mLatencyTuner;
};


#endif
