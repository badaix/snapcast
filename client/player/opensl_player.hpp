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

#ifndef OPEN_SL_PLAYER_HPP
#define OPEN_SL_PLAYER_HPP

// local headers
#include "player.hpp"

// 3rd party headers
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

// standard headers
#include <string>


namespace player
{

static constexpr auto OPENSL = "opensl";

typedef int (*AndroidAudioCallback)(short* buffer, int num_samples);


/// OpenSL Audio Player
/**
 * Player implementation for OpenSL (e.g. Android)
 */
class OpenslPlayer : public Player
{
public:
    OpenslPlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    virtual ~OpenslPlayer();

    void start() override;
    void stop() override;

    void playerCallback(SLAndroidSimpleBufferQueueItf bq);

protected:
    void initOpensl();
    void uninitOpensl();

    bool needsThread() const override;
    void throwUnsuccess(const std::string& phase, const std::string& what, SLresult result);
    std::string resultToString(SLresult result) const;

    // engine interfaces
    SLObjectItf engineObject;
    SLEngineItf engineEngine;
    SLObjectItf outputMixObject;

    // buffer queue player interfaces
    SLObjectItf bqPlayerObject;
    SLPlayItf bqPlayerPlay;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
    SLVolumeItf bqPlayerVolume;

    // Double buffering.
    int curBuffer;
    char* buffer[2];

    size_t ms_;
    size_t frames_;
    size_t buff_size;
    std::shared_ptr<Stream> pubStream_;
};

} // namespace player

#endif
