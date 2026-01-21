/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl
    Copyright (C) 2025  malkstar

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
#include <SDL2/SDL.h>

// standard headers
#include <atomic>
#include <memory>


namespace player
{

static constexpr auto SDL2 = "sdl2";

/// SDL2 Audio Player
/**
 * Audio player implementation using SDL2
 * Based on moonlight-tv's approach to webOS audio streaming
 */
class Sdl2Player : public Player
{
public:
    /// c'tor
    Sdl2Player(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    /// d'tor
    virtual ~Sdl2Player();

    void start() override;
    void stop() override;

protected:
    bool needsThread() const override;

private:
    /// SDL audio callback function
    static void audio_callback(void* userdata, Uint8* stream, int len);

    /// audio callback function
    void audioCallback(void* stream, int len);

    /// Initialize SDL audio subsystem
    bool initializeAudio();

    /// Cleanup SDL audio resources
    void cleanupAudio();

    SDL_AudioDeviceID audio_device_;
    SDL_AudioSpec audio_spec_;
    std::atomic<bool> initialized_;
};

} // namespace player