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

#ifndef ALSA_PLAYER_H
#define ALSA_PLAYER_H

#include "player.hpp"
#include <alsa/asoundlib.h>


/// Audio Player
/**
 * Audio player implementation using Alsa
 */
class AlsaPlayer : public Player
{
public:
    AlsaPlayer(const PcmDevice& pcmDevice, std::shared_ptr<Stream> stream);
    ~AlsaPlayer() override;

    /// Set audio volume in range [0..1]
    void start() override;
    void stop() override;

    /// List the system's audio output devices
    static std::vector<PcmDevice> pcm_list(void);
    void setVolume(double volume) override;

protected:
    void worker() override;

private:
    void initAlsa();
    void uninitAlsa();
    void initMixer();
    bool getVolume(double& volume, bool& muted);
    void openMixer(snd_mixer_elem_t** elem, snd_mixer_t** mixer);

    snd_pcm_t* handle_;

    snd_ctl_t* ctl_;

    pollfd* fd_;
    std::vector<char> buffer_;
    snd_pcm_uframes_t frames_;
};


#endif
