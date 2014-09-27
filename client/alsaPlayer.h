#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <alsa/asoundlib.h>
#include "stream.h"
#include "pcmDevice.h"


class Player
{
public:
	Player(const PcmDevice& pcmDevice, Stream* stream);
	void start();
	void stop();
	static std::vector<PcmDevice> pcm_list(void);

private:
	void worker();
	snd_pcm_t* pcm_handle;
	snd_pcm_uframes_t frames;
	char *buff;
	std::atomic<bool> active_;
	Stream* stream_;
	std::thread* playerThread;
	PcmDevice pcmDevice_;
};


#endif

