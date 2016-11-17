/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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
#include "stream.h"
#include "pcmDevice.h"
#include "common/endian.h"
#include "common/log.h"


/// Audio Player
/**
 * Abstract audio player implementation
 */
class Player
{
public:
	Player(const PcmDevice& pcmDevice, std::shared_ptr<Stream> stream);
	virtual ~Player();

	/// Set audio volume in range [0..1]
	virtual void setVolume(double volume);
	virtual void setMute(bool mute);
	virtual void start();
	virtual void stop();

protected:
	virtual void worker() = 0;

	template <typename T>
	void adjustVolume(char *buffer, size_t count, double volume)
	{
		T* bufferT = (T*)buffer;
		for (size_t n=0; n<count; ++n)
			bufferT[n] = endian::swap<T>(endian::swap<T>(bufferT[n]) * volume);
	}

	void adjustVolume(char* buffer, size_t frames);

	std::atomic<bool> active_;
	std::shared_ptr<Stream> stream_;
	std::thread playerThread_;
	PcmDevice pcmDevice_;
	double volume_;
	bool muted_;
	double volCorrection_;
};


#endif

