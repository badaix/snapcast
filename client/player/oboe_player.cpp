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

#include <assert.h>
#include <iostream>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "oboe_player.hpp"

using namespace std;


OboePlayer::OboePlayer(const PcmDevice& pcmDevice, std::shared_ptr<Stream> stream) : Player(pcmDevice, stream), ms_(50), buff_size(0), pubStream_(stream)
{

    oboe::AudioStreamBuilder builder;
    // The builder set methods can be chained for convenience.
    builder.setSharingMode(oboe::SharingMode::Exclusive)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setChannelCount(stream->getFormat().channels)
        ->setSampleRate(stream->getFormat().rate)
        ->setFormat(oboe::AudioFormat::I16)
        ->setCallback(this)
        ->openManagedStream(out_stream_);
}


OboePlayer::~OboePlayer()
{
}


oboe::DataCallbackResult OboePlayer::onAudioReady(oboe::AudioStream* oboeStream, void* audioData, int32_t numFrames)
{
    
    int32_t buffer_fill_size = oboeStream->getBufferCapacityInFrames() - numFrames;
    double delay_ms = static_cast<double>(buffer_fill_size) / stream_->getFormat().msRate();
    chronos::usec delay(static_cast<int>(delay_ms * 1000.));
    //LOG(INFO) << "onAudioReady frames: " << numFrames << ", capacity: " << oboeStream->getBufferCapacityInFrames() << ", size: " << oboeStream->getBufferSizeInFrames() << ", delay ms: " << delay_ms << "\n";

    if (!stream_->getPlayerChunk(audioData, delay, numFrames))
    {
        //		LOG(INFO) << "Failed to get chunk. Playing silence.\n";
        memset(audioData, 0, numFrames);
    }
    else
    {
        adjustVolume(static_cast<char*>(audioData), numFrames);
    }

    return oboe::DataCallbackResult::Continue;
}


void OboePlayer::start()
{
    // Typically, start the stream after querying some stream information, as well as some input from the user
    out_stream_->requestStart();
}


void OboePlayer::stop()
{
    out_stream_->requestStop();
}


void OboePlayer::worker()
{
}
