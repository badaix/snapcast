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

#include "player.hpp"

class WASAPIPlayer : public Player
{
public:
    WASAPIPlayer(const PcmDevice& pcmDevice, std::shared_ptr<Stream> stream);
	virtual ~WASAPIPlayer();
	
	static std::vector<PcmDevice> pcm_list(void);
protected:
	virtual void worker();
};

#endif
