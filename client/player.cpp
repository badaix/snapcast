#include "player.h"
#include <alsa/asoundlib.h>
#include <iostream>

#define PCM_DEVICE "default"

using namespace std;


Player::Player(Stream* stream) : active_(false), stream_(stream)
{
}


void Player::start()
{
	unsigned int pcm, tmp, rate;
	int channels;
	snd_pcm_hw_params_t *params;
	int buff_size;

	rate 	 = stream_->format.rate;
	channels = stream_->format.channels;


	/* Open the PCM device in playback mode */
	if ((pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) 
		cout << "ERROR: Can't open " << PCM_DEVICE << " PCM device. " << snd_strerror(pcm) << "\n";

/*	struct snd_pcm_playback_info_t pinfo;
	if ( (pcm = snd_pcm_playback_info( pcm_handle, &pinfo )) < 0 )
		fprintf( stderr, "Error: playback info error: %s\n", snd_strerror( err ) );
	printf("buffer: '%d'\n", pinfo.buffer_size);
*/
	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle, params);

	/* Set parameters */
	if ((pcm = snd_pcm_hw_params_set_access(pcm_handle, params,	SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		cout << "ERROR: Can't set interleaved mode. " << snd_strerror(pcm) << "\n";

	if ((pcm = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE)) < 0) 
		cout << "ERROR: Can't set format. " << snd_strerror(pcm) << "\n";

	if ((pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels)) < 0)
		cout << "ERROR: Can't set channels number. " << snd_strerror(pcm) << "\n";

	if ((pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0)) < 0) 
		cout << "ERROR: Can't set rate. " << snd_strerror(pcm) << "\n";

	long unsigned int periodsize = 2*rate/50;
	if ((pcm = snd_pcm_hw_params_set_buffer_size_near(pcm_handle, params, &periodsize)) < 0)
		cout << "Unable to set buffer size " << (long int)periodsize << ": " <<  snd_strerror(pcm) << "\n";

	/* Write parameters */
	if ((pcm = snd_pcm_hw_params(pcm_handle, params)) < 0)
		cout << "ERROR: Can't set harware parameters. " << snd_strerror(pcm) << "\n";

	/* Resume information */
	cout << "PCM name: " << snd_pcm_name(pcm_handle) << "\n";

	cout << "PCM state: " << snd_pcm_state_name(snd_pcm_state(pcm_handle)) << "\n";

	snd_pcm_hw_params_get_channels(params, &tmp);
	cout << "channels: " << tmp << "\n";

	if (tmp == 1)
		printf("(mono)\n");
	else if (tmp == 2)
		printf("(stereo)\n");

	snd_pcm_hw_params_get_rate(params, &tmp, 0);
	cout << "rate: " << tmp << " bps\n";

	/* Allocate buffer to hold single period */
	snd_pcm_hw_params_get_period_size(params, &frames, 0);
	cout << "frames: " << frames << "\n";

	buff_size = frames * channels * 2 /* 2 -> sample size */;
	buff = (char *) malloc(buff_size);

	snd_pcm_hw_params_get_period_time(params, &tmp, NULL);
	cout << "period time: " << tmp << "\n";

	playerThread = new thread(&Player::worker, this);
}


void Player::stop()
{
	active_ = false;
}


void Player::worker()
{
	unsigned int pcm;
	active_ = true;	
	while (active_)
	{
		snd_pcm_sframes_t avail;
		snd_pcm_sframes_t delay; 
		snd_pcm_avail_delay(pcm_handle, &avail, &delay);

		stream_->getPlayerChunk(buff, (float)delay / stream_->format.msRate(), frames);

		if ((pcm = snd_pcm_writei(pcm_handle, buff, frames)) == -EPIPE) {
			printf("XRUN.\n");
			snd_pcm_prepare(pcm_handle);
		} else if (pcm < 0) {
			printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
		}
	}

	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
	free(buff);
}


