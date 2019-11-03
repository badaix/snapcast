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
#include "common/utils/string_utils.hpp"

#define ID_OPUS 0x4F505553

namespace
{
template <typename T>
void assign(void* pointer, T val)
{
    T* p = (T*)pointer;
    *p = val;
}
} // namespace

// TODO:
// - handle variable chunk durations (now it's fixed to 10ms)

// const msg::SampleFormat& format);
OpusEncoder::OpusEncoder(const std::string& codecOptions) : Encoder(codecOptions), enc_(nullptr)
{
    headerChunk_.reset(new msg::CodecHeader("opus"));
}


OpusEncoder::~OpusEncoder()
{
    if (enc_ != nullptr)
        opus_encoder_destroy(enc_);
}


std::string OpusEncoder::getAvailableOptions() const
{
    return "BR:[6 - 512|MAX|AUTO],COMPLEXITY:[1-10]";
}


std::string OpusEncoder::getDefaultOptions() const
{
    return "BR:192,COMPLEXITY:10";
}

std::string OpusEncoder::name() const
{
    return "opus";
}


void OpusEncoder::initEncoder()
{
    opus_int32 bitrate = 192000;
    opus_int32 complexity = 10;
    auto options = utils::string::split(codecOptions_, ',');
    for (const auto& option : options)
    {
        auto kv = utils::string::split(option, ':');
        if (kv.size() == 2)
        {
            if (kv.front() == "BR")
            {
                if (kv.back() == "MAX")
                    bitrate = OPUS_BITRATE_MAX;
                else if (kv.back() == "AUTO")
                    bitrate = OPUS_AUTO;
                else
                {
                    try
                    {
                        bitrate = cpt::stoi(kv.back());
                        if ((bitrate < 6) || (bitrate > 512))
                            throw SnapException("Opus bitrate must be between 6 and 512");
                        bitrate *= 1000;
                    }
                    catch (const std::invalid_argument&)
                    {
                        throw SnapException("Opus error parsing bitrate (must be between 6 and 512): " + kv.back());
                    }
                }
            }
            else if (kv.front() == "COMPLEXITY")
            {
                try
                {
                    complexity = cpt::stoi(kv.back());
                    if ((complexity < 1) || (complexity > 10))
                        throw SnapException("Opus complexity must be between 1 and 10");
                }
                catch (const std::invalid_argument&)
                {
                    throw SnapException("Opus error parsing complexity (must be between 1 and 10): " + kv.back());
                }
            }
            else
                throw SnapException("Opus unknown option: " + kv.front());
        }
        else
            throw SnapException("Opus error parsing options: " + codecOptions_);
    }

    LOG(INFO) << "Opus bitrate: " << bitrate << " bps, complexity: " << complexity << "\n";

    int error;
    enc_ = opus_encoder_create(sampleFormat_.rate, sampleFormat_.channels, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error);
    if (error != 0)
    {
        throw SnapException("Failed to initialize Opus encoder: " + cpt::to_string(error));
    }

    opus_encoder_ctl(enc_, OPUS_SET_BITRATE(bitrate));       // max 512000, OPUS_BITRATE_MAX, OPUS_AUTO
    opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(complexity)); // max 10

    if ((sampleFormat_.rate != 48000) || (sampleFormat_.bits != 16) || (sampleFormat_.channels != 2))
        throw SnapException("Opus sampleformat must be 48000:16:2");

    // create some opus pseudo header to let the decoder know about the sample format
    headerChunk_->payloadSize = 12;
    headerChunk_->payload = (char*)malloc(headerChunk_->payloadSize);
    char* payload = headerChunk_->payload;
    assign(payload, SWAP_32(ID_OPUS));
    assign(payload + 4, SWAP_32(sampleFormat_.rate));
    assign(payload + 8, SWAP_16(sampleFormat_.bits));
    assign(payload + 10, SWAP_16(sampleFormat_.channels));
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
