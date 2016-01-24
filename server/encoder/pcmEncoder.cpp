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
	//TODO: Endianess
	headerChunk_->payloadSize = 44;
	headerChunk_->payload = (char*)malloc(headerChunk_->payloadSize);
	memcpy(headerChunk_->payload, "RIFF", 4);
	uint32_t int32 = 36;
	memcpy(headerChunk_->payload + 4, reinterpret_cast<const char *>(&int32), sizeof(uint32_t));
	memcpy(headerChunk_->payload + 8, "WAVEfmt ", 8);
	int32 = 16;
	memcpy(headerChunk_->payload + 16, reinterpret_cast<const char *>(&int32), sizeof(uint32_t));
	uint16_t int16 = 1;
	memcpy(headerChunk_->payload + 20, reinterpret_cast<const char *>(&int16), sizeof(uint16_t));
	int16 = sampleFormat_.channels;
	memcpy(headerChunk_->payload + 22, reinterpret_cast<const char *>(&int16), sizeof(uint16_t));
	int32 = sampleFormat_.rate;
	memcpy(headerChunk_->payload + 24, reinterpret_cast<const char *>(&int32), sizeof(uint32_t));
	int32 = sampleFormat_.rate * sampleFormat_.bits * sampleFormat_.channels / 8;
	memcpy(headerChunk_->payload + 28, reinterpret_cast<const char *>(&int32), sizeof(uint32_t));
	int16 = sampleFormat_.channels * ((sampleFormat_.bits + 7) / 8);
	memcpy(headerChunk_->payload + 32, reinterpret_cast<const char *>(&int16), sizeof(uint16_t));
	int16 = sampleFormat_.bits;
	memcpy(headerChunk_->payload + 34, reinterpret_cast<const char *>(&int16), sizeof(uint16_t));
	memcpy(headerChunk_->payload + 36, "data", 4);
	int32 = 0;
	memcpy(headerChunk_->payload + 40, reinterpret_cast<const char *>(&int32), sizeof(uint32_t));
}


std::string PcmEncoder::name() const
{
	return "pcm";
}


