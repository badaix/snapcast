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
#include "../stream.h"
#include "pcmDevice.h"


/// Audio Player
/**
 * Abstract audio player implementation
 */
class Player
{
public:
	Player(const PcmDevice& pcmDevice, Stream* stream);
	virtual ~Player();

	/// Set audio volume in range [0..1]
	virtual void setVolume(double volume);
	virtual void setMute(bool mute);
	virtual void start();
	virtual void stop();

protected:
	virtual void worker() = 0;

	std::atomic<bool> active_;
	Stream* stream_;
	std::thread playerThread_;
	PcmDevice pcmDevice_;
	double volume_;
	bool muted_;
};


#endif

