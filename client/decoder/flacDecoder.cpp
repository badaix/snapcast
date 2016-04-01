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

#include <iostream>
#include <cstring>
#include <cmath>
#include <FLAC/stream_decoder.h>
#include "flacDecoder.h"
#include "common/snapException.h"
#include "common/endian.h"
#include "common/log.h"


using namespace std;


static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);


static msg::Header* flacHeader = NULL;
static msg::PcmChunk* flacChunk = NULL;
static msg::PcmChunk* pcmChunk = NULL;
static SampleFormat sampleFormat;
static FLAC__StreamDecoder *decoder = NULL;



FlacDecoder::FlacDecoder() : Decoder()
{
	flacChunk = new msg::PcmChunk();
}


FlacDecoder::~FlacDecoder()
{
	std::lock_guard<std::mutex> lock(mutex_);
	delete flacChunk;
	delete decoder;
}


bool FlacDecoder::decode(msg::PcmChunk* chunk)
{
	std::lock_guard<std::mutex> lock(mutex_);
	cacheInfo_.reset();
	pcmChunk = chunk;
	flacChunk->payload = (char*)realloc(flacChunk->payload, chunk->payloadSize);
	memcpy(flacChunk->payload, chunk->payload, chunk->payloadSize);
	flacChunk->payloadSize = chunk->payloadSize;

	pcmChunk->payload = (char*)realloc(pcmChunk->payload, 0);
	pcmChunk->payloadSize = 0;
	while (flacChunk->payloadSize > 0)
		FLAC__stream_decoder_process_single(decoder);

	if ((cacheInfo_.cachedBlocks_ > 0) && (cacheInfo_.sampleRate_ != 0))
	{
		double diffMs = cacheInfo_.cachedBlocks_ / ((double)cacheInfo_.sampleRate_ / 1000.);
		int32_t s = (diffMs / 1000);
		int32_t us = (diffMs * 1000);
		us %= 1000000;
		logD << "Cached: " << cacheInfo_.cachedBlocks_ << ", " << diffMs << "ms, " << s << "s, " << us << "us\n";
		chunk->timestamp = chunk->timestamp - tv(s, us);
	}
	return true;
}


SampleFormat FlacDecoder::setHeader(msg::Header* chunk)
{
	flacHeader = chunk;
	FLAC__StreamDecoderInitStatus init_status;

	if ((decoder = FLAC__stream_decoder_new()) == NULL)
		throw SnapException("ERROR: allocating decoder");

//	(void)FLAC__stream_decoder_set_md5_checking(decoder, true);
	init_status = FLAC__stream_decoder_init_stream(decoder, read_callback, NULL, NULL, NULL, NULL, write_callback, metadata_callback, error_callback, this);
	if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		throw SnapException("ERROR: initializing decoder: " + string(FLAC__StreamDecoderInitStatusString[init_status]));

	sampleFormat.rate = 0;
	FLAC__stream_decoder_process_until_end_of_metadata(decoder);
	if (sampleFormat.rate == 0)
		throw SnapException("Sample format not found");

	return sampleFormat;
}


FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	if (flacHeader != NULL)
	{
		*bytes = flacHeader->payloadSize;
		memcpy(buffer, flacHeader->payload, *bytes);
		flacHeader = NULL;
	}
	else if (flacChunk != NULL)
	{
		static_cast<FlacDecoder*>(client_data)->cacheInfo_.isCachedChunk_ = false;
		if (*bytes > flacChunk->payloadSize)
			*bytes = flacChunk->payloadSize;

		memcpy(buffer, flacChunk->payload, *bytes);
		memmove(flacChunk->payload, flacChunk->payload + *bytes, flacChunk->payloadSize - *bytes);
		flacChunk->payloadSize = flacChunk->payloadSize - *bytes;
		flacChunk->payload = (char*)realloc(flacChunk->payload, flacChunk->payloadSize);
	}
	return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}


FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	(void)decoder;

/*	if(channels != 2 || bps != 16) {
		fprintf(stderr, "ERROR: this example only supports 16bit stereo streams\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	if(frame->header.channels != 2) {
		fprintf(stderr, "ERROR: This frame contains %d channels (should be 2)\n", frame->header.channels);
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
*/	if (buffer[0] == NULL)
	{
		logS(kLogErr) << "ERROR: buffer [0] is NULL\n";
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (buffer[1] == NULL)
	{
		logS(kLogErr) << "ERROR: buffer [1] is NULL\n";
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (pcmChunk != NULL)
	{
//TODO: hard coded to 2 channels, 16 bit
		size_t bytes = frame->header.blocksize * 4;

		FlacDecoder* flacDecoder = static_cast<FlacDecoder*>(client_data);
		if (flacDecoder->cacheInfo_.isCachedChunk_)
			flacDecoder->cacheInfo_.cachedBlocks_ += frame->header.blocksize;

		pcmChunk->payload = (char*)realloc(pcmChunk->payload, pcmChunk->payloadSize + bytes);

		int16_t* pcm = (int16_t*)(pcmChunk->payload + pcmChunk->payloadSize);
		for(size_t i = 0; i < frame->header.blocksize; i++)
		{
			pcm[2*i] = SWAP_16((int16_t)(buffer[0][i]));
			pcm[2*i + 1] = SWAP_16((int16_t)(buffer[1][i]));
//			memcpy(pcmChunk->payload + pcmChunk->payloadSize + 4*i, (char*)(buffer[0] + i), 2);
//			memcpy(pcmChunk->payload + pcmChunk->payloadSize + 4*i+2, (char*)(buffer[1] + i), 2);
		}
		pcmChunk->payloadSize += bytes;
	}

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	(void)decoder;
	/* print some stats */
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		static_cast<FlacDecoder*>(client_data)->cacheInfo_.sampleRate_ = metadata->data.stream_info.sample_rate;
		sampleFormat.setFormat(
			metadata->data.stream_info.sample_rate,
			metadata->data.stream_info.bits_per_sample,
			metadata->data.stream_info.channels);
	}
}


void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;
	logS(kLogErr) << "Got error callback: " << FLAC__StreamDecoderErrorStatusString[status] << "\n";
}





