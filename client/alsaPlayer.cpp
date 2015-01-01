#include "alsaPlayer.h"
#include "common/log.h"
#include <alsa/asoundlib.h>
#include <iostream>

#define BUFFER_TIME 100000

using namespace std;

Player::Player(const PcmDevice& pcmDevice, Stream* stream) : pcm_handle_(NULL), buff_(NULL), active_(false), stream_(stream), pcmDevice_(pcmDevice) 
{
}


void Player::start() 
{
	unsigned int pcm, tmp, rate;
	int channels;
	snd_pcm_hw_params_t *params;
	int buff_size;

	const msg::SampleFormat& format = stream_->getFormat();
	rate = format.rate;
	channels = format.channels;

	/* Open the PCM device in playback mode */
	if ((pcm = snd_pcm_open(&pcm_handle_, pcmDevice_.name.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)
		logE << "ERROR: Can't open " << pcmDevice_.name << " PCM device. " << snd_strerror(pcm) << "\n";

	/*	struct snd_pcm_playback_info_t pinfo;
	 if ( (pcm = snd_pcm_playback_info( pcm_handle, &pinfo )) < 0 )
	 fprintf( stderr, "Error: playback info error: %s\n", snd_strerror( err ) );
	 printf("buffer: '%d'\n", pinfo.buffer_size);
	 */
	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle_, params);

	/* Set parameters */
	if ((pcm = snd_pcm_hw_params_set_access(pcm_handle_, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		logE << "ERROR: Can't set interleaved mode. " << snd_strerror(pcm) << "\n";

	if ((pcm = snd_pcm_hw_params_set_format(pcm_handle_, params, SND_PCM_FORMAT_S16_LE)) < 0)
		logE << "ERROR: Can't set format. " << snd_strerror(pcm) << "\n";

	if ((pcm = snd_pcm_hw_params_set_channels(pcm_handle_, params, channels)) < 0)
		logE << "ERROR: Can't set channels number. " << snd_strerror(pcm) << "\n";

	if ((pcm = snd_pcm_hw_params_set_rate_near(pcm_handle_, params, &rate, 0)) < 0)
		logE << "ERROR: Can't set rate. " << snd_strerror(pcm) << "\n";

	unsigned int buffer_time;
	snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
	if (buffer_time > BUFFER_TIME)
		buffer_time = BUFFER_TIME;

	unsigned int period_time = buffer_time / 4;

	snd_pcm_hw_params_set_period_time_near(pcm_handle_, params, &period_time, 0);
	snd_pcm_hw_params_set_buffer_time_near(pcm_handle_, params, &buffer_time, 0);

//	long unsigned int periodsize = stream_->format.msRate() * 50;//2*rate/50;
//	if ((pcm = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &periodsize)) < 0)
//		logE << "Unable to set buffer size " << (long int)periodsize << ": " <<  snd_strerror(pcm) << "\n";

	/* Write parameters */
	if ((pcm = snd_pcm_hw_params(pcm_handle_, params)) < 0)
		logE << "ERROR: Can't set harware parameters. " << snd_strerror(pcm) << "\n";

	/* Resume information */
	logD << "PCM name: " << snd_pcm_name(pcm_handle_) << "\n";
	logD << "PCM state: " << snd_pcm_state_name(snd_pcm_state(pcm_handle_)) << "\n";
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
	snd_pcm_sw_params_current(pcm_handle_, swparams);

	snd_pcm_sw_params_set_avail_min(pcm_handle_, swparams, frames_);
	snd_pcm_sw_params_set_start_threshold(pcm_handle_, swparams, frames_);
//	snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams, frames_);
	snd_pcm_sw_params(pcm_handle_, swparams);

	active_ = true;
	playerThread_ = new thread(&Player::worker, this);
}


Player::~Player()
{
	stop();
}


void Player::stop() {
	active_ = false;
	if (playerThread_ != NULL)
	{
		playerThread_->join();
		delete playerThread_;
		playerThread_ = NULL;
	}

	if (pcm_handle_ != NULL)
	{
		snd_pcm_drain(pcm_handle_);
		snd_pcm_close(pcm_handle_);
		pcm_handle_ = NULL;
	}
	
	if (buff_ != NULL)
	{
		free(buff_);
		buff_ = NULL;
	}
}


void Player::worker() 
{
	unsigned int pcm;
	snd_pcm_sframes_t framesAvail;
	snd_pcm_sframes_t framesDelay;
	while (active_) 
	{
		snd_pcm_avail_delay(pcm_handle_, &framesAvail, &framesDelay);
		chronos::usec delay((chronos::usec::rep) (1000 * (double) framesDelay / stream_->getFormat().msRate()));
		logD << "Avail: " << framesAvail << ", delay: " << framesDelay << ", delay[ms]: " << delay.count() / 1000 << "\n";

		if (stream_->getPlayerChunk(buff_, delay, frames_)) 
		{
			if ((pcm = snd_pcm_writei(pcm_handle_, buff_, frames_)) == -EPIPE) 
			{
				logE << "XRUN\n";
				snd_pcm_prepare(pcm_handle_);
			} 
			else if (pcm < 0) 
			{
				logE << "ERROR. Can't write to PCM device: " << snd_strerror(pcm) << "\n";
			}
		}
		else
			usleep(100*1000);
	}
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

