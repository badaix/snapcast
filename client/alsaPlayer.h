/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <alsa/asoundlib.h>
#include "stream.h"
#include "pcmDevice.h"


/// Audio Player
/**
 * Audio player implementation using Alsa
 */
class Player
{
public:
	Player(const PcmDevice& pcmDevice, Stream* stream);
	virtual ~Player();

	/// Set audio volume in range [0..1]
	void setVolume(double volume);
	void setMute(bool mute);
	void start();
	void stop();

	/// List the system's audio output devices
	static std::vector<PcmDevice> pcm_list(void);

private:
	void initAlsa();
	void uninitAlsa();
	void worker();

	template <typename T>
	void adjustVolume(char *buffer, size_t count, double volume)
	{
		T* bufferT = (T*)buffer;
		for (size_t n=0; n<count; ++n)
			bufferT[n] *= volume;
	}

	snd_pcm_t* handle_;
	snd_pcm_uframes_t frames_;

	char *buff_;
	std::atomic<bool> active_;
	Stream* stream_;
	std::thread playerThread_;
	PcmDevice pcmDevice_;
	double volume_;
	bool muted_;
};


#endif

