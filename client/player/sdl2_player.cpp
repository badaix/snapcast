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

// prototype/interface header file
#include "sdl2_player.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"

// standard headers
#include <chrono>
#include <cstring>
#include <iostream>

using namespace std;

namespace player
{

static constexpr auto LOG_TAG = "SDL2Player";
static constexpr auto LATENCY = 30ms;

Sdl2Player::Sdl2Player(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream)
    : Player(io_context, settings, std::move(stream)), audio_device_(0), initialized_(false)
{
    LOG(INFO, LOG_TAG) << "Sdl2Player created\n";
}


Sdl2Player::~Sdl2Player()
{
    stop();
    cleanupAudio();
    LOG(INFO, LOG_TAG) << "Sdl2Player destroyed\n";
}


bool Sdl2Player::needsThread() const
{
    return false;
}


void Sdl2Player::start()
{
    LOG(INFO, LOG_TAG) << "Starting SDL2 player\n";

    if (!initializeAudio())
        throw SnapException("Failed to initialize SDL2 audio");

    Player::start();

    // Start SDL audio playback
    SDL_PauseAudioDevice(audio_device_, 0);

    LOG(INFO, LOG_TAG) << "SDL2 player started successfully\n";
}


void Sdl2Player::stop()
{
    LOG(INFO, LOG_TAG) << "Stopping SDL2 player\n";

    if (audio_device_ != 0)
        SDL_PauseAudioDevice(audio_device_, 1);

    Player::stop();

    LOG(INFO, LOG_TAG) << "SDL2 player stopped\n";
}


bool Sdl2Player::initializeAudio()
{
    LOG(INFO, LOG_TAG) << "Initializing SDL audio subsystem\n";

    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        LOG(ERROR, LOG_TAG) << "Failed to initialize SDL audio: " << SDL_GetError() << "\n";
        return false;
    }

    // Get the stream format
    const auto& format = stream_->getFormat();
    if (format.bits() != 16)
        throw SnapException("Unsupported sample format: " + cpt::to_string(format.bits()));

    // Configure SDL audio specification
    SDL_zero(audio_spec_);
    audio_spec_.freq = format.rate();
    audio_spec_.format = AUDIO_S16LSB; // 16-bit signed little-endian
    audio_spec_.channels = format.channels();
    audio_spec_.samples = 1024; // Buffer size in samples
    audio_spec_.callback = audio_callback;
    audio_spec_.userdata = this;

    LOG(INFO, LOG_TAG) << "Audio format: " << format.rate() << "Hz, " << format.channels() << " channels, " << format.bits() << " bits\n";

    // Open audio device
    SDL_AudioSpec obtained_spec;
    audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &audio_spec_, &obtained_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

    if (audio_device_ == 0)
    {
        LOG(ERROR, LOG_TAG) << "Failed to open audio device: " << SDL_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    LOG(INFO, LOG_TAG) << "Obtained audio format: " << obtained_spec.freq << "Hz, " << (int)obtained_spec.channels << " channels\n";

    // Update our spec with what we actually got
    audio_spec_ = obtained_spec;
    initialized_ = true;

    return true;
}


void Sdl2Player::cleanupAudio()
{
    if (audio_device_ != 0)
    {
        SDL_CloseAudioDevice(audio_device_);
        audio_device_ = 0;
    }

    if (initialized_)
    {
        SDL_Quit();
        initialized_ = false;
    }

    LOG(INFO, LOG_TAG) << "SDL audio cleaned up\n";
}


void Sdl2Player::audio_callback(void* userdata, Uint8* stream, int len)
{
    auto* player = static_cast<Sdl2Player*>(userdata);
    if (!player)
    {
        // Fill with silence if not active
        SDL_memset(stream, 0, len);
        return;
    }

    player->audioCallback(stream, len);
}


void Sdl2Player::audioCallback(void* stream, int len)
{
    const auto& fmt = stream_->getFormat();
    auto frames = len / fmt.frameSize();
    auto latency = std::chrono::milliseconds(static_cast<size_t>(audio_spec_.samples / fmt.msRate()));
    latency += LATENCY;

    // LOG(DEBUG, LOG_TAG) << "audioCallback: " << frames << " frames\n";
    if (!stream_->getPlayerChunkOrSilence(stream, latency, frames))
    {
        // LOG(INFO, LOG_TAG) << "Failed to get chunk. Playing silence.\n";
    }
    else
    {
        adjustVolume(static_cast<char*>(stream), frames);
    }
}


} // namespace player