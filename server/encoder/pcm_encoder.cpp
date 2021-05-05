/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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

#include "pcm_encoder.hpp"
#include "common/aixlog.hpp"
#include "common/endian.hpp"
#include <memory>


namespace encoder
{

static constexpr auto ID_RIFF = 0x46464952;
static constexpr auto ID_WAVE = 0x45564157;
static constexpr auto ID_FMT = 0x20746d66;
static constexpr auto ID_DATA = 0x61746164;

static constexpr auto LOG_TAG = "PcmEnc";

namespace
{
template <typename T>
void assign(void* pointer, T val)
{
    T* p = (T*)pointer;
    *p = val;
}
} // namespace


PcmEncoder::PcmEncoder(const std::string& codecOptions) : Encoder(codecOptions)
{
    headerChunk_.reset(new msg::CodecHeader("pcm"));
}


void PcmEncoder::encode(const msg::PcmChunk& chunk)
{
    // copy the chunk into a shared_ptr
    auto pcmChunk = std::make_shared<msg::PcmChunk>(chunk);
    encoded_callback_(*this, pcmChunk, pcmChunk->durationMs());
}


void PcmEncoder::initEncoder()
{
    LOG(INFO, LOG_TAG) << "Init\n";
    headerChunk_->payloadSize = 44;
    headerChunk_->payload = static_cast<char*>(realloc(headerChunk_->payload, headerChunk_->payloadSize));
    char* payload = headerChunk_->payload;
    assign(payload, SWAP_32(ID_RIFF));
    assign(payload + 4, SWAP_32(36));
    assign(payload + 8, SWAP_32(ID_WAVE));
    assign(payload + 12, SWAP_32(ID_FMT));
    assign(payload + 16, SWAP_32(16));
    assign(payload + 20, SWAP_16(1));
    assign(payload + 22, SWAP_16(sampleFormat_.channels()));
    assign(payload + 24, SWAP_32(sampleFormat_.rate()));
    assign(payload + 28, SWAP_32(sampleFormat_.rate() * sampleFormat_.bits() * sampleFormat_.channels() / 8));
    assign(payload + 32, SWAP_16(sampleFormat_.channels() * ((sampleFormat_.bits() + 7) / 8)));
    assign(payload + 34, SWAP_16(sampleFormat_.bits()));
    assign(payload + 36, SWAP_32(ID_DATA));
    assign(payload + 40, SWAP_32(0));
}


std::string PcmEncoder::name() const
{
    return "pcm";
}

} // namespace encoder
