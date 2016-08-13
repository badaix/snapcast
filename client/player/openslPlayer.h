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

#ifndef OPEN_SL_PLAYER_H
#define OPEN_SL_PLAYER_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <string>

#include "player.h"

typedef int (*AndroidAudioCallback)(short *buffer, int num_samples);


/// OpenSL Audio Player
/**
 * Player implementation for OpenSL (e.g. Android)
 */
class OpenslPlayer : public Player
{
public:
	OpenslPlayer(const PcmDevice& pcmDevice, Stream* stream);
	virtual ~OpenslPlayer();

	virtual void start();
	virtual void stop();

	void playerCallback(SLAndroidSimpleBufferQueueItf bq);

protected:
	void initOpensl();
	void uninitOpensl();

	virtual void worker();
	void throwUnsuccess(const std::string& what, SLresult result);
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
	char *buffer[2];

	size_t ms_;
	size_t frames_;
	size_t buff_size;
	Stream* pubStream_;
};


#endif

