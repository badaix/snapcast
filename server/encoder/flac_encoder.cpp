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

#include <iostream>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "flac_encoder.hpp"

using namespace std;

namespace encoder
{

static constexpr auto LOG_TAG = "FlacEnc";

FlacEncoder::FlacEncoder(const std::string& codecOptions) : Encoder(codecOptions), encoder_(nullptr), pcmBufferSize_(0), encodedSamples_(0), flacChunk_(nullptr)
{
    headerChunk_.reset(new msg::CodecHeader("flac"));
    pcmBuffer_ = static_cast<FLAC__int32*>(malloc(pcmBufferSize_ * sizeof(FLAC__int32)));
    metadata_[0] = nullptr;
    metadata_[1] = nullptr;
}


FlacEncoder::~FlacEncoder()
{
    if (encoder_ != nullptr)
    {
        FLAC__stream_encoder_finish(encoder_);
        FLAC__metadata_object_delete(metadata_[0]);
        FLAC__metadata_object_delete(metadata_[1]);
        FLAC__stream_encoder_delete(encoder_);
    }

    free(pcmBuffer_);
}


std::string FlacEncoder::getAvailableOptions() const
{
    return "compression level: [0..8]";
}


std::string FlacEncoder::getDefaultOptions() const
{
    return "2";
}


std::string FlacEncoder::name() const
{
    return "flac";
}


void FlacEncoder::encode(const msg::PcmChunk& chunk)
{
    if (flacChunk_ == nullptr)
        flacChunk_ = make_shared<msg::PcmChunk>(chunk.format, 0);

    int samples = chunk.getSampleCount();
    int frames = chunk.getFrameCount();
    // LOG(TRACE, LOG_TAG) << "payload: " << chunk.payloadSize << "\tframes: " << frames << "\tsamples: " << samples
    //                     << "\tduration: " << chunk.duration<chronos::msec>().count() << ", format: " << chunk.format.toString() << "\n";

    if (pcmBufferSize_ < samples)
    {
        pcmBufferSize_ = samples;
        pcmBuffer_ = static_cast<FLAC__int32*>(realloc(pcmBuffer_, pcmBufferSize_ * sizeof(FLAC__int32)));
    }

    auto clip = [](int32_t min, int32_t max, int32_t value) -> int32_t
    {
        if (value < min)
            return min;
        if (value > max)
            return max;
        return value;
    };

    if (sampleFormat_.sampleSize() == 1)
    {
        auto* buffer = reinterpret_cast<FLAC__int8*>(chunk.payload);
        for (int i = 0; i < samples; i++)
            pcmBuffer_[i] = clip(-128, 127, static_cast<FLAC__int32>(buffer[i]));
    }
    else if (sampleFormat_.sampleSize() == 2)
    {
        auto* buffer = reinterpret_cast<FLAC__int16*>(chunk.payload);
        for (int i = 0; i < samples; i++)
            pcmBuffer_[i] = clip(-32768, 32767, static_cast<FLAC__int32>(buffer[i]));
    }
    else if (sampleFormat_.sampleSize() == 4)
    {
        auto* buffer = reinterpret_cast<FLAC__int32*>(chunk.payload);
        if (sampleFormat_.bits() == 24)
        {
            for (int i = 0; i < samples; i++)
                pcmBuffer_[i] = clip(-8388608, 8388607, buffer[i]);
        }
        else
        {
            for (int i = 0; i < samples; i++)
                pcmBuffer_[i] = buffer[i];
        }
    }


    FLAC__stream_encoder_process_interleaved(encoder_, pcmBuffer_, frames);

    if (encodedSamples_ > 0)
    {
        double resMs = static_cast<double>(encodedSamples_) / sampleFormat_.msRate();
        //		LOG(INFO, LOG_TAG) << "encoded: " << chunk->payloadSize << "\tframes: " << encodedSamples_ << "\tres: " << resMs << "\n";
        encodedSamples_ = 0;
        encoded_callback_(*this, flacChunk_, resMs);
        flacChunk_ = make_shared<msg::PcmChunk>(chunk.format, 0);
    }
}


FLAC__StreamEncoderWriteStatus FlacEncoder::write_callback(const FLAC__StreamEncoder* /*encoder*/, const FLAC__byte buffer[], size_t bytes, unsigned samples,
                                                           unsigned current_frame)
{
    //	LOG(INFO, LOG_TAG) << "write_callback: " << bytes << ", " << samples << ", " << current_frame << "\n";
    if ((current_frame == 0) && (bytes > 0) && (samples == 0))
    {
        headerChunk_->payload = static_cast<char*>(realloc(headerChunk_->payload, headerChunk_->payloadSize + bytes));
        memcpy(headerChunk_->payload + headerChunk_->payloadSize, buffer, bytes);
        headerChunk_->payloadSize += bytes;
    }
    else
    {
        flacChunk_->payload = static_cast<char*>(realloc(flacChunk_->payload, flacChunk_->payloadSize + bytes));
        memcpy(flacChunk_->payload + flacChunk_->payloadSize, buffer, bytes);
        flacChunk_->payloadSize += bytes;
        encodedSamples_ += samples;
    }
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

namespace callback
{
FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples,
                                              unsigned current_frame, void* client_data)
{
    auto* flacEncoder = static_cast<FlacEncoder*>(client_data);
    return flacEncoder->write_callback(encoder, buffer, bytes, samples, current_frame);
}
} // namespace callback

void FlacEncoder::initEncoder()
{
    int compression_level(2);
    try
    {
        compression_level = cpt::stoi(codecOptions_);
    }
    catch (...)
    {
        throw SnapException("Invalid codec option: \"" + codecOptions_ + "\"");
    }
    if ((compression_level < 0) || (compression_level > 8))
    {
        throw SnapException("compression level has to be between 0 and 8");
    }

    LOG(INFO, LOG_TAG) << "Init - compression level: " << compression_level << "\n";

    FLAC__bool ok = 1;
    FLAC__StreamEncoderInitStatus init_status;
    FLAC__StreamMetadata_VorbisComment_Entry entry;

    // allocate the encoder
    if ((encoder_ = FLAC__stream_encoder_new()) == nullptr)
        throw SnapException("error allocating encoder");

    ok &= FLAC__stream_encoder_set_verify(encoder_, 1);
    // compression levels (0-8):
    // https://xiph.org/flac/api/group__flac__stream__encoder.html#gae49cf32f5256cb47eecd33779493ac85
    // latency:
    // 0-2: 1152 frames, ~26.1224ms
    // 3-8: 4096 frames, ~92.8798ms
    ok &= FLAC__stream_encoder_set_compression_level(encoder_, compression_level);
    ok &= FLAC__stream_encoder_set_channels(encoder_, sampleFormat_.channels());
    ok &= FLAC__stream_encoder_set_bits_per_sample(encoder_, sampleFormat_.bits());
    ok &= FLAC__stream_encoder_set_sample_rate(encoder_, sampleFormat_.rate());

    if (ok == 0)
        throw SnapException("error setting up encoder");

    // now add some metadata; we'll add some tags and a padding block
    if ((metadata_[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) == nullptr ||
        (metadata_[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) == nullptr ||
        // there are many tag (vorbiscomment) functions but these are convenient for this particular use:
        (FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "TITLE", "SnapStream") == 0) ||
        (FLAC__metadata_object_vorbiscomment_append_comment(metadata_[0], entry, 0) == 0) ||
        (FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "VERSION", VERSION) == 0) ||
        (FLAC__metadata_object_vorbiscomment_append_comment(metadata_[0], entry, 0) == 0))
        throw SnapException("out of memory or tag error");

    metadata_[1]->length = 1234; // set the padding length
    ok = FLAC__stream_encoder_set_metadata(encoder_, metadata_, 2);
    if (ok == 0)
        throw SnapException("error setting meta data");

    // initialize encoder
    init_status = FLAC__stream_encoder_init_stream(encoder_, callback::write_callback, nullptr, nullptr, nullptr, this);
    if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        throw SnapException("ERROR: initializing encoder: " + string(FLAC__StreamEncoderInitStatusString[init_status]));
}

} // namespace encoder
