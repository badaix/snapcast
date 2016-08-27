#ifndef WASAPI_PLAYER_H
#define WASAPI_PLAYER_H

#include "player.h"

class WASAPIPlayer : public Player
{
public:
	WASAPIPlayer(const PcmDevice& pcmDevice, Stream* stream);
	virtual ~WASAPIPlayer();
	
	static std::vector<PcmDevice> pcm_list(void);
protected:
	virtual void worker();
};

#endif
