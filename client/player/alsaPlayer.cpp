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

#include "alsaPlayer.h"
#include "common/log.h"
#include "common/snapException.h"
#include "common/strCompat.h"

//#define BUFFER_TIME 120000
#define PERIOD_TIME 30000

using namespace std;

AlsaPlayer::AlsaPlayer(const PcmDevice& pcmDevice, Stream* stream) : 
	Player(pcmDevice, stream), handle_(NULL), buff_(NULL)
{
}


void AlsaPlayer::initAlsa()
{
	unsigned int tmp, rate;
	int pcm, channels;
	snd_pcm_hw_params_t *params;
	int buff_size;

	const SampleFormat& format = stream_->getFormat();
	rate = format.rate;
	channels = format.channels;

	/* Open the PCM device in playback mode */
	if ((pcm = snd_pcm_open(&handle_, pcmDevice_.name.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
		throw SnapException("Can't open " + pcmDevice_.name + " PCM device: " + snd_strerror(pcm));

	/*	struct snd_pcm_playback_info_t pinfo;
	 if ( (pcm = snd_pcm_playback_info( pcm_handle, &pinfo )) < 0 )
	 fprintf( stderr, "Error: playback info error: %s\n", snd_strerror( err ) );
	 printf("buffer: '%d'\n", pinfo.buffer_size);
	 */
	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	if ((pcm = snd_pcm_hw_params_any(handle_, params)) < 0)
		throw SnapException("Can't fill params: " + string(snd_strerror(pcm)));

	/* Set parameters */
	if ((pcm = snd_pcm_hw_params_set_access(handle_, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		throw SnapException("Can't set interleaved mode: " + string(snd_strerror(pcm)));

	snd_pcm_format_t snd_pcm_format;
	if (format.bits == 8)
		snd_pcm_format = SND_PCM_FORMAT_S8;
	else if (format.bits == 16)
		snd_pcm_format = SND_PCM_FORMAT_S16_LE;
	else if ((format.bits == 24) && (format.sampleSize == 4)) 
		snd_pcm_format = SND_PCM_FORMAT_S24_LE;
	else if (format.bits == 32)
		snd_pcm_format = SND_PCM_FORMAT_S32_LE;
	else
		throw SnapException("Unsupported sample format: "  + cpt::to_string(format.bits));

	pcm = snd_pcm_hw_params_set_format(handle_, params, snd_pcm_format);
	if (pcm == -EINVAL)
	{
		if (snd_pcm_format == SND_PCM_FORMAT_S24_LE)
		{
			snd_pcm_format = SND_PCM_FORMAT_S32_LE;
			volCorrection_ = 256;
		}
		if (snd_pcm_format == SND_PCM_FORMAT_S8)
		{
			snd_pcm_format = SND_PCM_FORMAT_U8;
		}
	}
	
	pcm = snd_pcm_hw_params_set_format(handle_, params, snd_pcm_format);
	if (pcm < 0)
	{
		cerr << "error: " << pcm << "\n";
		stringstream ss;
		ss << "Can't set format: " << string(snd_strerror(pcm)) << ", supported: ";
		for (int format = 0; format <= (int)SND_PCM_FORMAT_LAST; format++)
		{
			snd_pcm_format_t snd_pcm_format = static_cast<snd_pcm_format_t>(format);
 			if (snd_pcm_hw_params_test_format(handle_, params, snd_pcm_format) == 0)
 				ss << snd_pcm_format_name(snd_pcm_format) << " ";
		}
 		throw SnapException(ss.str());
	}

	if ((pcm = snd_pcm_hw_params_set_channels(handle_, params, channels)) < 0)
		throw SnapException("Can't set channels number: " + string(snd_strerror(pcm)));

	if ((pcm = snd_pcm_hw_params_set_rate_near(handle_, params, &rate, 0)) < 0)
		throw SnapException("Can't set rate: " + string(snd_strerror(pcm)));

	unsigned int period_time;
	snd_pcm_hw_params_get_period_time_max(params, &period_time, 0);
	if (period_time > PERIOD_TIME)
		period_time = PERIOD_TIME;

	unsigned int buffer_time = 4 * period_time;

	snd_pcm_hw_params_set_period_time_near(handle_, params, &period_time, 0);
	snd_pcm_hw_params_set_buffer_time_near(handle_, params, &buffer_time, 0);

//	long unsigned int periodsize = stream_->format.msRate() * 50;//2*rate/50;
//	if ((pcm = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &periodsize)) < 0)
//		logE << "Unable to set buffer size " << (long int)periodsize << ": " <<  snd_strerror(pcm) << "\n";

	/* Write parameters */
	if ((pcm = snd_pcm_hw_params(handle_, params)) < 0)
		throw SnapException("Can't set harware parameters: " + string(snd_strerror(pcm)));

	/* Resume information */
	logD << "PCM name: " << snd_pcm_name(handle_) << "\n";
	logD << "PCM state: " << snd_pcm_state_name(snd_pcm_state(handle_)) << "\n";
	snd_pcm_hw_params_get_channels(params, &tmp);
	logD << "channels: " << tmp << "\n";

	snd_pcm_hw_params_get_rate(params, &tmp, 0);
	logD << "rate: " << tmp << " bps\n";

	/* Allocate buffer to hold single period */
	snd_pcm_hw_params_get_period_size(params, &frames_, 0);
	logO << "frames: " << frames_ << "\n";

	buff_size = frames_ * format.frameSize; //channels * 2 /* 2 -> sample size */;
	buff_ = (char *) malloc(buff_size);

	snd_pcm_hw_params_get_period_time(params, &tmp, NULL);
	logD << "period time: " << tmp << "\n";

	snd_pcm_sw_params_t *swparams;
	snd_pcm_sw_params_alloca(&swparams);
	snd_pcm_sw_params_current(handle_, swparams);

	snd_pcm_sw_params_set_avail_min(handle_, swparams, frames_);
	snd_pcm_sw_params_set_start_threshold(handle_, swparams, frames_);
//	snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams, frames_);
	snd_pcm_sw_params(handle_, swparams);
}


void AlsaPlayer::uninitAlsa()
{
	if (handle_ != NULL)
	{
		snd_pcm_drain(handle_);
		snd_pcm_close(handle_);
		handle_ = NULL;
	}

	if (buff_ != NULL)
	{
		free(buff_);
		buff_ = NULL;
	}
}


void AlsaPlayer::start()
{
	initAlsa();
	Player::start();
}


AlsaPlayer::~AlsaPlayer()
{
	stop();
}


void AlsaPlayer::stop()
{
	Player::stop();
	uninitAlsa();
}


void AlsaPlayer::worker()
{
	snd_pcm_sframes_t pcm;
	snd_pcm_sframes_t framesDelay;
	long lastChunkTick = 0;

	while (active_)
	{
		if (handle_ == NULL)
		{
			try
			{
				initAlsa();
			}
			catch (const std::exception& e)
			{
				logE << "Exception in initAlsa: " << e.what() << endl;
				usleep(100*1000);
			}
		}

//		snd_pcm_avail_delay(handle_, &framesAvail, &framesDelay);
		snd_pcm_delay(handle_, &framesDelay);
		chronos::usec delay((chronos::usec::rep) (1000 * (double) framesDelay / stream_->getFormat().msRate()));
//		logO << "delay: " << framesDelay << ", delay[ms]: " << delay.count() / 1000 << "\n";

		if (stream_->getPlayerChunk(buff_, delay, frames_))
		{
			lastChunkTick = chronos::getTickCount();
			adjustVolume(buff_, frames_);
			if ((pcm = snd_pcm_writei(handle_, buff_, frames_)) == -EPIPE)
			{
				logE << "XRUN\n";
				snd_pcm_prepare(handle_);
			}
			else if (pcm < 0)
			{
				logE << "ERROR. Can't write to PCM device: " << snd_strerror(pcm) << "\n";
				uninitAlsa();
			}
		}
		else
		{
			logO << "Failed to get chunk\n";
			while (active_ && !stream_->waitForChunk(100))
			{
				logD << "Waiting for chunk\n";
				if ((handle_ != NULL) && (chronos::getTickCount() - lastChunkTick > 5000))
				{
					logO << "No chunk received for 5000ms. Closing ALSA.\n";
					uninitAlsa();
					stream_->clearChunks();
				}
			}
		}
	}
}



vector<PcmDevice> AlsaPlayer::pcm_list(void)
{
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
		if(descr == NULL) {
			pcmDevice.description = "";
		} else {
			pcmDevice.description = descr;
		}
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

