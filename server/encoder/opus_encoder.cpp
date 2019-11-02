/***
        This file is part of snapcast
        Copyright (C) 2015  Hannes Ellinger

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

#include "opus_encoder.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"


// const msg::SampleFormat& format);
OpusEncoder::OpusEncoder(const std::string& codecOptions) : Encoder(codecOptions), enc_(nullptr)
{
}


OpusEncoder::~OpusEncoder()
{
    if (enc_ != nullptr)
        opus_encoder_destroy(enc_);
}


std::string OpusEncoder::name() const
{
    return "opus";
}


void OpusEncoder::initEncoder()
{
    int error;
    enc_ = opus_encoder_create(sampleFormat_.rate, sampleFormat_.channels, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error);
    if (error != 0)
    {
        throw SnapException("Failed to initialize opus encoder: " + cpt::to_string(error));
    }
    headerChunk_.reset(new msg::CodecHeader("opus"));
}


void OpusEncoder::encode(const msg::PcmChunk* chunk)
{
    int samples = chunk->payloadSize / 4;
    if (encoded_.size() < chunk->payloadSize)
        encoded_.resize(chunk->payloadSize);

    // encode
    opus_int32 len = opus_encode(enc_, (opus_int16*)chunk->payload, samples, encoded_.data(), chunk->payloadSize);
    LOG(DEBUG) << "Encoded: size " << chunk->payloadSize << " bytes, encoded: " << len << " bytes" << '\n';

    if (len > 0)
    {
        // copy encoded data to chunk
        auto* opusChunk = new msg::PcmChunk(chunk->format, 0);
        opusChunk->payloadSize = len;
        opusChunk->payload = (char*)realloc(opusChunk->payload, opusChunk->payloadSize);
        memcpy(opusChunk->payload, encoded_.data(), len);
        listener_->onChunkEncoded(this, opusChunk, (double)samples / ((double)sampleFormat_.rate / 1000.));
    }
    else
    {
        LOG(ERROR) << "Failed to encode chunk: " << len << '\n' << " Frame size: " << samples / 2 << '\n' << " Max bytes:  " << chunk->payloadSize << '\n';
    }
}
