/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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

#include <iostream>
#include <cmath>

#include "player.h"
#include "aixlog.hpp"


using namespace std;


Player::Player(const PcmDevice& pcmDevice, std::shared_ptr<Stream> stream) :
	active_(false),
	stream_(stream),
	pcmDevice_(pcmDevice),
	volume_(1.0),
	muted_(false),
	volCorrection_(1.0)
{
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


void Player::adjustVolume(char* buffer, size_t frames)
{
	double volume = volume_;
	if (muted_)
		volume = 0.;

	const SampleFormat& sampleFormat = stream_->getFormat();

	if ((volume < 1.0) || (volCorrection_ != 1.))
	{
		volume *= volCorrection_;
		if (sampleFormat.sampleSize == 1)
			adjustVolume<int8_t>(buffer, frames*sampleFormat.channels, volume);
		else if (sampleFormat.sampleSize == 2)
			adjustVolume<int16_t>(buffer, frames*sampleFormat.channels, volume);
		else if (sampleFormat.sampleSize == 4)
			adjustVolume<int32_t>(buffer, frames*sampleFormat.channels, volume);
	}
}


//https://cgit.freedesktop.org/pulseaudio/pulseaudio/tree/src/pulse/volume.c#n260
//http://www.robotplanet.dk/audio/audio_gui_design/
//https://lists.linuxaudio.org/pipermail/linux-audio-dev/2009-May/thread.html#22198
void Player::setVolume_poly(double volume, double exp)
{
	volume_ = std::pow(volume, exp);
	LOG(DEBUG) << "setVolume poly: " << volume << " => " << volume_ << "\n";
}


//http://stackoverflow.com/questions/1165026/what-algorithms-could-i-use-for-audio-volume-level
void Player::setVolume_exp(double volume, double base)
{
//	double base = M_E;
//	double base = 10.;
	volume_ = (pow(base, volume)-1) / (base-1);
	LOG(DEBUG) << "setVolume exp: " << volume << " => " << volume_ << "\n";
}


void Player::setVolume(double volume)
{
	setVolume_exp(volume, 10.);
}


void Player::setMute(bool mute)
{
	muted_ = mute;
}


