/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef CORE_AUDIO_PLAYER_HPP
#define CORE_AUDIO_PLAYER_HPP

// local headers
#include "player.hpp"

// 3rd party headers
#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CFRunLoop.h>


namespace player
{

static constexpr auto COREAUDIO = "coreaudio";

/// Audio Player
/**
 * Audio player implementation using CoreAudio
 *
 * Warning:
 *
 *  !! This player is experimental and might not be maintained !!
 *
 */
class CoreAudioPlayer : public Player
{
public:
    CoreAudioPlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    virtual ~CoreAudioPlayer();

    void playerCallback(AudioQueueRef queue, AudioQueueBufferRef bufferRef);
    static std::vector<PcmDevice> pcm_list();

protected:
    void worker() override;
    bool needsThread() const override;

    void initAudioQueue();
    void uninitAudioQueue(AudioQueueRef queue);

    AudioQueueTimelineRef timeLine_;
    size_t ms_;
    size_t frames_;
    size_t buff_size_;
    std::shared_ptr<Stream> pubStream_;
    long lastChunkTick;
};

} // namespace player

#endif
