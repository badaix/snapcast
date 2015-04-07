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

#include "alsaPlayer.h"
#include "common/log.h"
#include <alsa/asoundlib.h>
#include <iostream>

#define BUFFER_TIME 100000

using namespace std;

Player::Player(const PcmDevice& pcmDevice, Stream* stream) : buff_(NULL), active_(false), stream_(stream), pcmDevice_(pcmDevice) 
{
}


snd_pcm_t* Player::initAlsa() 
{
	unsigned int tmp, rate;
	int pcm, channels;
	snd_pcm_hw_params_t *params;
	int buff_size;

	const msg::SampleFormat& format = stream_->getFormat();
	rate = format.rate;
	channels = format.channels;

	snd_pcm_t* pcm_handle(NULL);
	
	/* Open the PCM device in playback mode */
	if ((pcm = snd_pcm_open(&pcm_handle, pcmDevice_.name.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
	{
		logE << "ERROR: Can't open " << pcmDevice_.name << " PCM device. " << snd_strerror(pcm) << "\n";
		return NULL;
	}
	/*	struct snd_pcm_playback_info_t pinfo;
	 if ( (pcm = snd_pcm_playback_info( pcm_handle, &pinfo )) < 0 )
	 fprintf( stderr, "Error: playback info error: %s\n", snd_strerror( err ) );
	 printf("buffer: '%d'\n", pinfo.buffer_size);
	 */
	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle, params);

	/* Set parameters */
	if ((pcm = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
	{
		logE << "ERROR: Can't set interleaved mode. " << snd_strerror(pcm) << "\n";
		return NULL;
	}

	if ((pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE)) < 0)
	{
		logE << "ERROR: Can't set format. " << snd_strerror(pcm) << "\n";
		return NULL;
	}

	if ((pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels)) < 0)
	{
		logE << "ERROR: Can't set channels number. " << snd_strerror(pcm) << "\n";
		return NULL;
	}

	if ((pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0)) < 0)
	{
		logE << "ERROR: Can't set rate. " << snd_strerror(pcm) << "\n";
		return NULL;
	}

	unsigned int buffer_time;
	snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
	if (buffer_time > BUFFER_TIME)
		buffer_time = BUFFER_TIME;

	unsigned int period_time = buffer_time / 4;

	snd_pcm_hw_params_set_period_time_near(pcm_handle, params, &period_time, 0);
	snd_pcm_hw_params_set_buffer_time_near(pcm_handle, params, &buffer_time, 0);

//	long unsigned int periodsize = stream_->format.msRate() * 50;//2*rate/50;
//	if ((pcm = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &periodsize)) < 0)
//		logE << "Unable to set buffer size " << (long int)periodsize << ": " <<  snd_strerror(pcm) << "\n";

	/* Write parameters */
	if ((pcm = snd_pcm_hw_params(pcm_handle, params)) < 0)
	{
		logE << "ERROR: Can't set harware parameters. " << snd_strerror(pcm) << "\n";
		return NULL;
	}

	/* Resume information */
	logD << "PCM name: " << snd_pcm_name(pcm_handle) << "\n";
	logD << "PCM state: " << snd_pcm_state_name(snd_pcm_state(pcm_handle)) << "\n";
	snd_pcm_hw_params_get_channels(params, &tmp);
	logD << "channels: " << tmp << "\n";

	snd_pcm_hw_params_get_rate(params, &tmp, 0);
	logD << "rate: " << tmp << " bps\n";

	/* Allocate buffer to hold single period */
	snd_pcm_hw_params_get_period_size(params, &frames_, 0);
	logD << "frames: " << frames_ << "\n";

	buff_size = frames_ * channels * 2 /* 2 -> sample size */;
	buff_ = (char *) malloc(buff_size);

	snd_pcm_hw_params_get_period_time(params, &tmp, NULL);
	logD << "period time: " << tmp << "\n";

	snd_pcm_sw_params_t *swparams;
	snd_pcm_sw_params_alloca(&swparams);
	snd_pcm_sw_params_current(pcm_handle, swparams);

	snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, frames_);
	snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, frames_);
//	snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams, frames_);
	snd_pcm_sw_params(pcm_handle, swparams);
	return pcm_handle;
}


void Player::uninitAlsa(snd_pcm_t* handle)
{
	if (handle != NULL)
	{
		snd_pcm_drain(handle);
		snd_pcm_close(handle);
	}
	
	if (buff_ != NULL)
	{
		free(buff_);
		buff_ = NULL;
	}
}


void Player::start() 
{
	active_ = true;
	playerThread_ = thread(&Player::worker, this);
}


Player::~Player()
{
	stop();
}


void Player::stop() 
{
	if (active_)
	{
		active_ = false;
		playerThread_.join();
	}
}


void Player::worker() 
{
	snd_pcm_t* handle = NULL;
	snd_pcm_sframes_t pcm;
	snd_pcm_sframes_t framesAvail;
	snd_pcm_sframes_t framesDelay;
	while (active_) 
	{
		if (handle == NULL)
		{
			if ((handle = initAlsa()) == NULL)
			{
				usleep(100*1000);
				continue;
			}
		}
		
		snd_pcm_avail_delay(handle, &framesAvail, &framesDelay);
		chronos::usec delay((chronos::usec::rep) (1000 * (double) framesDelay / stream_->getFormat().msRate()));
		logD << "Avail: " << framesAvail << ", delay: " << framesDelay << ", delay[ms]: " << delay.count() / 1000 << "\n";

		if (stream_->getPlayerChunk(buff_, delay, frames_)) 
		{
			if ((pcm = snd_pcm_writei(handle, buff_, frames_)) == -EPIPE) 
			{
				logE << "XRUN\n";
				snd_pcm_prepare(handle);
			} 
			else if (pcm < 0) 
			{
				logE << "ERROR. Can't write to PCM device: " << snd_strerror(pcm) << "\n";
				uninitAlsa(handle);
				handle = NULL;
			}
		}
		else
		{
			logO << "Failed to get chunk\n";
			usleep(100*1000);
		}
	}
	uninitAlsa(handle);
}


vector<PcmDevice> Player::pcm_list(void) {
	void **hints, **n;
	char *name, *descr, *io;
	vector<PcmDevice> result;
	PcmDevice pcmDevice;

	if (snd_device_name_hint(-1, "pcm", &hints) < 0)
		return result;
	n = hints;
	size_t idx(0);
	while (*n != NULL) 
	{
		name = snd_device_name_get_hint(*n, "NAME");
		descr = snd_device_name_get_hint(*n, "DESC");
		io = snd_device_name_get_hint(*n, "IOID");
		if (io != NULL && strcmp(io, "Output") != 0)
			goto __end;
		pcmDevice.name = name;
		pcmDevice.description = descr;
		pcmDevice.idx = idx++;
		result.push_back(pcmDevice);

__end: 
		if (name != NULL)
			free(name);
		if (descr != NULL)
			free(descr);
		if (io != NULL)
			free(io);
		n++;
	}
	snd_device_name_free_hint(hints);
	return result;
}

