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

#ifndef WASAPI_PLAYER_H
#define WASAPI_PLAYER_H

#include <audioclient.h>
#include <mmdeviceapi.h>

#include "player.h"

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

class WASAPIPlayer : public Player
{
public:
	WASAPIPlayer(const PcmDevice& pcmDevice, Stream* stream);
	virtual ~WASAPIPlayer();

	virtual void start();
	virtual void stop();
protected:
	virtual void worker();
private:
	IMMDeviceEnumerator* deviceEnumerator = NULL;
	IMMDevice* device = NULL;
	IAudioClient* audioClient = NULL;
	WAVEFORMATEX* waveformat = NULL;
	WAVEFORMATEXTENSIBLE* waveformatExtended = NULL;
	IAudioRenderClient* renderClient = NULL;
	IAudioClock* clock = NULL;
	UINT32 bufferFrameCount;
	HANDLE taskHandle;
	HANDLE eventHandle;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;

	bool wasapiActive = false;
	
	void initWasapi(void);
	void uninitWasapi(void);
};

#endif
