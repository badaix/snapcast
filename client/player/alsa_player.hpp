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

#ifndef ALSA_PLAYER_HPP
#define ALSA_PLAYER_HPP

// local headers
#include "player.hpp"

// 3rd party headers
#include <alsa/asoundlib.h>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <chrono>
#include <optional>


namespace player
{

static constexpr auto ALSA = "alsa";

/// Audio Player
/**
 * Audio player implementation using Alsa
 */
class AlsaPlayer : public Player
{
public:
    AlsaPlayer(boost::asio::io_context& io_context, const ClientSettings::Player& settings, std::shared_ptr<Stream> stream);
    ~AlsaPlayer() override;

    void start() override;
    void stop() override;

    /// List the system's audio output devices
    static std::vector<PcmDevice> pcm_list();

protected:
    void worker() override;
    bool needsThread() const override;

private:
    /// initialize alsa and the mixer (if neccessary)
    void initAlsa();
    /// free alsa and optionally the mixer
    /// @param uninit_mixer free the mixer
    void uninitAlsa(bool uninit_mixer);
    bool getAvailDelay(snd_pcm_sframes_t& avail, snd_pcm_sframes_t& delay);

    void initMixer();
    void uninitMixer();

    bool getHardwareVolume(double& volume, bool& muted) override;
    void setHardwareVolume(double volume, bool muted) override;

    void waitForEvent();

    snd_pcm_t* handle_;
    snd_ctl_t* ctl_;

    snd_mixer_t* mixer_;
    snd_mixer_elem_t* elem_;
    std::string mixer_name_;
    std::string mixer_device_;

    std::unique_ptr<pollfd, std::function<void(pollfd*)>> fd_;
    std::vector<char> buffer_;
    snd_pcm_uframes_t frames_;
    boost::asio::posix::stream_descriptor sd_;
    std::chrono::time_point<std::chrono::steady_clock> last_change_;
    std::recursive_mutex mutex_;
    boost::asio::steady_timer timer_;

    std::optional<std::chrono::microseconds> buffer_time_;
    std::optional<uint32_t> periods_;
};

} // namespace player

#endif