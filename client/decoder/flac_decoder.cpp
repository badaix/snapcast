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

#include "flac_decoder.hpp"
#include "common/aixlog.hpp"
#include "common/endian.hpp"
#include "common/snap_exception.hpp"
#include <cmath>
#include <cstring>
#include <iostream>


using namespace std;

static constexpr auto LOG_TAG = "FlacDecoder";

namespace decoder
{

namespace callback
{
FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder* decoder, FLAC__byte buffer[], size_t* bytes, void* client_data);
FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder* decoder, const FLAC__Frame* frame, const FLAC__int32* const buffer[],
                                              void* client_data);
void metadata_callback(const FLAC__StreamDecoder* decoder, const FLAC__StreamMetadata* metadata, void* client_data);
void error_callback(const FLAC__StreamDecoder* decoder, FLAC__StreamDecoderErrorStatus status, void* client_data);
} // namespace callback

namespace
{
msg::CodecHeader* flacHeader = nullptr;
msg::PcmChunk* flacChunk = nullptr;
msg::PcmChunk* pcmChunk = nullptr;
SampleFormat sampleFormat;
FLAC__StreamDecoder* decoder = nullptr;
} // namespace


FlacDecoder::FlacDecoder() : Decoder(), lastError_(nullptr)
{
    flacChunk = new msg::PcmChunk();
}


FlacDecoder::~FlacDecoder()
{
    std::lock_guard<std::mutex> lock(mutex_);
    FLAC__stream_decoder_delete(decoder);
    delete flacChunk;
}


bool FlacDecoder::decode(msg::PcmChunk* chunk)
{
    std::lock_guard<std::mutex> lock(mutex_);
    cacheInfo_.reset();
    pcmChunk = chunk;
    flacChunk->payload = static_cast<char*>(realloc(flacChunk->payload, chunk->payloadSize));
    memcpy(flacChunk->payload, chunk->payload, chunk->payloadSize);
    flacChunk->payloadSize = chunk->payloadSize;

    pcmChunk->payload = static_cast<char*>(realloc(pcmChunk->payload, 0));
    pcmChunk->payloadSize = 0;
    while (flacChunk->payloadSize > 0)
    {
        if (FLAC__stream_decoder_process_single(decoder) == 0)
        {
            return false;
        }

        if (lastError_)
        {
            LOG(ERROR, LOG_TAG) << "FLAC decode error: " << FLAC__StreamDecoderErrorStatusString[*lastError_] << "\n";
            lastError_ = nullptr;
            return false;
        }
    }

    if ((cacheInfo_.cachedBlocks_ > 0) && (cacheInfo_.sampleRate_ != 0))
    {
        double diffMs = static_cast<double>(cacheInfo_.cachedBlocks_) / (static_cast<double>(cacheInfo_.sampleRate_) / 1000.);
        auto us = static_cast<uint64_t>(diffMs * 1000.);
        tv diff(static_cast<int32_t>(us / 1000000), static_cast<int32_t>(us % 1000000));
        LOG(TRACE, LOG_TAG) << "Cached: " << cacheInfo_.cachedBlocks_ << ", " << diffMs << "ms, " << diff.sec << "s, " << diff.usec << "us\n";
        chunk->timestamp = chunk->timestamp - diff;
    }
    return true;
}


SampleFormat FlacDecoder::setHeader(msg::CodecHeader* chunk)
{
    flacHeader = chunk;
    FLAC__StreamDecoderInitStatus init_status;

    if ((decoder = FLAC__stream_decoder_new()) == nullptr)
        throw SnapException("ERROR: allocating decoder");

    //	(void)FLAC__stream_decoder_set_md5_checking(decoder, true);
    init_status = FLAC__stream_decoder_init_stream(decoder, callback::read_callback, nullptr, nullptr, nullptr, nullptr, callback::write_callback,
                                                   callback::metadata_callback, callback::error_callback, this);
    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
        throw SnapException("ERROR: initializing decoder: " + string(FLAC__StreamDecoderInitStatusString[init_status]));

    FLAC__stream_decoder_process_until_end_of_metadata(decoder);
    if (sampleFormat.rate() == 0)
        throw SnapException("Sample format not found");

    return sampleFormat;
}

namespace callback
{
FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[], size_t* bytes, void* client_data)
{
    if (flacHeader != nullptr)
    {
        *bytes = flacHeader->payloadSize;
        memcpy(buffer, flacHeader->payload, *bytes);
        flacHeader = nullptr;
    }
    else if (flacChunk != nullptr)
    {
        //		cerr << "read_callback: " << *bytes << ", avail: " << flacChunk->payloadSize << "\n";
        static_cast<FlacDecoder*>(client_data)->cacheInfo_.isCachedChunk_ = false;
        if (*bytes > flacChunk->payloadSize)
            *bytes = flacChunk->payloadSize;

        //		if (*bytes == 0)
        //			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

        memcpy(buffer, flacChunk->payload, *bytes);
        memmove(flacChunk->payload, flacChunk->payload + *bytes, flacChunk->payloadSize - *bytes);
        flacChunk->payloadSize = flacChunk->payloadSize - static_cast<uint32_t>(*bytes);
        flacChunk->payload = static_cast<char*>(realloc(flacChunk->payload, flacChunk->payloadSize));
    }
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}


FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame, const FLAC__int32* const buffer[],
                                              void* client_data)
{
    if (pcmChunk != nullptr)
    {
        size_t bytes = frame->header.blocksize * sampleFormat.frameSize();

        auto* flacDecoder = static_cast<FlacDecoder*>(client_data);
        if (flacDecoder->cacheInfo_.isCachedChunk_)
            flacDecoder->cacheInfo_.cachedBlocks_ += frame->header.blocksize;

        pcmChunk->payload = static_cast<char*>(realloc(pcmChunk->payload, pcmChunk->payloadSize + bytes));

        for (size_t channel = 0; channel < sampleFormat.channels(); ++channel)
        {
            if (buffer[channel] == nullptr)
            {
                LOG(ERROR, LOG_TAG) << "ERROR: buffer[" << channel << "] is NULL\n";
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }

            if (sampleFormat.sampleSize() == 1)
            {
                auto* chunkBuffer = reinterpret_cast<int8_t*>(pcmChunk->payload + pcmChunk->payloadSize);
                for (size_t i = 0; i < frame->header.blocksize; i++)
                    chunkBuffer[sampleFormat.channels() * i + channel] = static_cast<int8_t>(buffer[channel][i]);
            }
            else if (sampleFormat.sampleSize() == 2)
            {
                auto* chunkBuffer = reinterpret_cast<int16_t*>(pcmChunk->payload + pcmChunk->payloadSize);
                for (size_t i = 0; i < frame->header.blocksize; i++)
                    chunkBuffer[sampleFormat.channels() * i + channel] = SWAP_16((int16_t)(buffer[channel][i]));
            }
            else if (sampleFormat.sampleSize() == 4)
            {
                auto* chunkBuffer = reinterpret_cast<int32_t*>(pcmChunk->payload + pcmChunk->payloadSize);
                for (size_t i = 0; i < frame->header.blocksize; i++)
                    chunkBuffer[sampleFormat.channels() * i + channel] = SWAP_32((int32_t)(buffer[channel][i]));
            }
        }
        pcmChunk->payloadSize += static_cast<uint32_t>(bytes);
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


void metadata_callback(const FLAC__StreamDecoder* /*decoder*/, const FLAC__StreamMetadata* metadata, void* client_data)
{
    /* print some stats */
    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
    {
        static_cast<FlacDecoder*>(client_data)->cacheInfo_.sampleRate_ = metadata->data.stream_info.sample_rate;
        sampleFormat.setFormat(metadata->data.stream_info.sample_rate, static_cast<uint16_t>(metadata->data.stream_info.bits_per_sample),
                               static_cast<uint16_t>(metadata->data.stream_info.channels));
    }
}


void error_callback(const FLAC__StreamDecoder* /*decoder*/, FLAC__StreamDecoderErrorStatus status, void* client_data)
{
    LOG(ERROR, LOG_TAG) << "Got error callback: " << FLAC__StreamDecoderErrorStatusString[status] << "\n";
    static_cast<FlacDecoder*>(client_data)->lastError_ = std::make_unique<FLAC__StreamDecoderErrorStatus>(status);
}
} // namespace callback

} // namespace decoder
