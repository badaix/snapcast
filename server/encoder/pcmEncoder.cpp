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

#include <memory>
#include "pcmEncoder.h"

PcmEncoder::PcmEncoder(const std::string& codecOptions) : Encoder(codecOptions)
{
	headerChunk_ = new msg::Header("pcm");
}


void PcmEncoder::encode(const msg::PcmChunk* chunk)
{
	msg::PcmChunk* pcmChunk = new msg::PcmChunk(*chunk);
	listener_->onChunkEncoded(this, pcmChunk, pcmChunk->duration<chronos::msec>().count());
}


void PcmEncoder::initEncoder()
{
}


std::string PcmEncoder::name() const
{
	return "pcm";
}


