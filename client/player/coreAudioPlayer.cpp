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

#include <unistd.h>

#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CFRunLoop.h>

#include "coreAudioPlayer.h"


//http://stackoverflow.com/questions/4863811/how-to-use-audioqueue-to-play-a-sound-for-mac-osx-in-c
//https://gist.github.com/andormade/1360885


CoreAudioPlayer::CoreAudioPlayer(const PcmDevice& pcmDevice, Stream* stream) : Player(pcmDevice, stream)
{
}


CoreAudioPlayer::~CoreAudioPlayer()
{
}


void CoreAudioPlayer::worker()
{
	while (active_)
	{
		usleep(100*1000);
	}
}


