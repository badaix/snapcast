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
#include "flacDecoder.h"
#include "common/snapException.h"
#include "common/endian.h"
#include "common/log.h"


using namespace std;


static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);


static msg::CodecHeader* flacHeader = NULL;
static msg::PcmChunk* flacChunk = NULL;
static msg::PcmChunk* pcmChunk = NULL;
static SampleFormat sampleFormat;
static FLAC__StreamDecoder *decoder = NULL;



FlacDecoder::FlacDecoder() : Decoder(), lastError_(nullptr)
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
	{
		if (!FLAC__stream_decoder_process_single(decoder))
		{
			return false;
		}

		if (lastError_)
		{
			logE << "FLAC decode error: " << FLAC__StreamDecoderErrorStatusString[*lastError_] << "\n";
			lastError_= nullptr;
			return false;
		}
	}

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


SampleFormat FlacDecoder::setHeader(msg::CodecHeader* chunk)
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
//		cerr << "read_callback: " << *bytes << ", avail: " << flacChunk->payloadSize << "\n";
		static_cast<FlacDecoder*>(client_data)->cacheInfo_.isCachedChunk_ = false;
		if (*bytes > flacChunk->payloadSize)
			*bytes = flacChunk->payloadSize;

//		if (*bytes == 0)
//			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

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

	if (pcmChunk != NULL)
	{
		size_t bytes = frame->header.blocksize * sampleFormat.frameSize;

		FlacDecoder* flacDecoder = static_cast<FlacDecoder*>(client_data);
		if (flacDecoder->cacheInfo_.isCachedChunk_)
			flacDecoder->cacheInfo_.cachedBlocks_ += frame->header.blocksize;

		pcmChunk->payload = (char*)realloc(pcmChunk->payload, pcmChunk->payloadSize + bytes);

		for (size_t channel = 0; channel < sampleFormat.channels; ++channel)
		{
			if (buffer[channel] == NULL)
			{
				logS(kLogErr) << "ERROR: buffer[" << channel << "] is NULL\n";
				return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
			}
			
			if (sampleFormat.sampleSize == 1)
			{
				int8_t* chunkBuffer = (int8_t*)(pcmChunk->payload + pcmChunk->payloadSize);
				for (size_t i = 0; i < frame->header.blocksize; i++)
					chunkBuffer[sampleFormat.channels*i + channel] = (int8_t)(buffer[channel][i]);
			}
			else if (sampleFormat.sampleSize == 2)
			{
				int16_t* chunkBuffer = (int16_t*)(pcmChunk->payload + pcmChunk->payloadSize);
				for (size_t i = 0; i < frame->header.blocksize; i++)
					chunkBuffer[sampleFormat.channels*i + channel] = SWAP_16((int16_t)(buffer[channel][i]));
			}
			else if (sampleFormat.sampleSize == 4)
			{
				int32_t* chunkBuffer = (int32_t*)(pcmChunk->payload + pcmChunk->payloadSize);
				for (size_t i = 0; i < frame->header.blocksize; i++)
					chunkBuffer[sampleFormat.channels*i + channel] = SWAP_32((int32_t)(buffer[channel][i]));
			}
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
	static_cast<FlacDecoder*>(client_data)->lastError_ = std::unique_ptr<FLAC__StreamDecoderErrorStatus>(new FLAC__StreamDecoderErrorStatus(status));

	/// TODO, see issue #120:
	// Thu Nov 10 07:26:44 2016 daemon.warn dnsmasq-dhcp[1194]: no address range available for DHCP request via wlan0
	// Thu Nov 10 07:54:39 2016 daemon.err snapclient[1158]: Got error callback: FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC
	// Thu Nov 10 07:54:39 2016 daemon.err snapclient[1158]: Got error callback: FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC
	//
	// and:
	// Oct 27 17:37:38 kitchen snapclient[869]: Connected to 192.168.222.10
	// Oct 27 17:47:13 kitchen snapclient[869]: Got error callback: FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM
	// Oct 27 17:47:13 kitchen snapclient[869]: Got error callback: FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC
}





