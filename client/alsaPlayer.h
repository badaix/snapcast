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
	virtual ~Player();
	void start();
	void stop();
	static std::vector<PcmDevice> pcm_list(void);

private:
	void worker();
	snd_pcm_t* pcm_handle_;
	snd_pcm_uframes_t frames_;
	char *buff_;
	std::atomic<bool> active_;
	Stream* stream_;
	std::thread playerThread_;
	PcmDevice pcmDevice_;
};


#endif

