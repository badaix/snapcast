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

#include "flacEncoder.h"
#include "common/strCompat.h"
#include "common/snapException.h"
#include "common/log.h"

using namespace std;


FlacEncoder::FlacEncoder(const std::string& codecOptions) : Encoder(codecOptions), encoder_(NULL), pcmBufferSize_(0), encodedSamples_(0)
{
	flacChunk_ = new msg::PcmChunk();
	headerChunk_.reset(new msg::CodecHeader("flac"));
	pcmBuffer_ = (FLAC__int32*)malloc(pcmBufferSize_ * sizeof(FLAC__int32));
}


FlacEncoder::~FlacEncoder()
{
	if (encoder_ != NULL)
	{
		FLAC__stream_encoder_finish(encoder_);
		FLAC__metadata_object_delete(metadata_[0]);
		FLAC__metadata_object_delete(metadata_[1]);
		FLAC__stream_encoder_delete(encoder_);
	}

	delete flacChunk_;
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


void FlacEncoder::encode(const msg::PcmChunk* chunk)
{
	int samples = chunk->getSampleCount();
	int frames = chunk->getFrameCount();
//	logO << "payload: " << chunk->payloadSize << "\tframes: " << frames << "\tsamples: " << samples << "\tduration: " << chunk->duration<chronos::msec>().count() << "\n";

	if (pcmBufferSize_ < samples)
	{
		pcmBufferSize_ = samples;
		pcmBuffer_ = (FLAC__int32*)realloc(pcmBuffer_, pcmBufferSize_ * sizeof(FLAC__int32));
	}

	if (sampleFormat_.sampleSize == 1)
	{
		FLAC__int8* buffer = (FLAC__int8*)chunk->payload;
		for(int i=0; i<samples; i++)
			pcmBuffer_[i] = (FLAC__int32)(buffer[i]);
	}
	else if (sampleFormat_.sampleSize == 2)
	{
		FLAC__int16* buffer = (FLAC__int16*)chunk->payload;
		for(int i=0; i<samples; i++)
			pcmBuffer_[i] = (FLAC__int32)(buffer[i]);
	}
	else if (sampleFormat_.sampleSize == 4)
	{
		FLAC__int32* buffer = (FLAC__int32*)chunk->payload;
		for(int i=0; i<samples; i++)
			pcmBuffer_[i] = (FLAC__int32)(buffer[i]);
	}


	FLAC__stream_encoder_process_interleaved(encoder_, pcmBuffer_, frames);

	if (encodedSamples_ > 0)
	{
		double resMs = encodedSamples_ / ((double)sampleFormat_.rate / 1000.);
//		logO << "encoded: " << chunk->payloadSize << "\tframes: " << encodedSamples_ << "\tres: " << resMs << "\n";
		encodedSamples_ = 0;
		listener_->onChunkEncoded(this, flacChunk_, resMs);
		flacChunk_ = new msg::PcmChunk(chunk->format, 0);
	}
}


FLAC__StreamEncoderWriteStatus FlacEncoder::write_callback(const FLAC__StreamEncoder *encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame)
{
//	logO << "write_callback: " << bytes << ", " << samples << ", " << current_frame << "\n";
	if ((current_frame == 0) && (bytes > 0) && (samples == 0))
	{
		headerChunk_->payload = (char*)realloc(headerChunk_->payload, headerChunk_->payloadSize + bytes);
		memcpy(headerChunk_->payload + headerChunk_->payloadSize, buffer, bytes);
		headerChunk_->payloadSize += bytes;
	}
	else
	{
		flacChunk_->payload = (char*)realloc(flacChunk_->payload, flacChunk_->payloadSize + bytes);
		memcpy(flacChunk_->payload + flacChunk_->payloadSize, buffer, bytes);
		flacChunk_->payloadSize += bytes;
		encodedSamples_ += samples;
	}
	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}


FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void *client_data)
{
	FlacEncoder* flacEncoder = (FlacEncoder*)client_data;
	return flacEncoder->write_callback(encoder, buffer, bytes, samples, current_frame);
}


void FlacEncoder::initEncoder()
{
	int quality(2);
	try
	{
		quality = cpt::stoi(codecOptions_);
	}
	catch(...)
	{
		throw SnapException("Invalid codec option: \"" + codecOptions_ + "\"");
	}
	if ((quality < 0) || (quality > 8))
	{
		throw SnapException("compression level has to be between 0 and 8");
	}

	FLAC__bool ok = true;
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata_VorbisComment_Entry entry;

	// allocate the encoder
	if ((encoder_ = FLAC__stream_encoder_new()) == NULL)
		throw SnapException("error allocating encoder");

	ok &= FLAC__stream_encoder_set_verify(encoder_, true);
	// compression levels (0-8):
	// https://xiph.org/flac/api/group__flac__stream__encoder.html#gae49cf32f5256cb47eecd33779493ac85
	// latency:
	// 0-2: 1152 frames, ~26.1224ms
	// 3-8: 4096 frames, ~92.8798ms
	ok &= FLAC__stream_encoder_set_compression_level(encoder_, quality);
	ok &= FLAC__stream_encoder_set_channels(encoder_, sampleFormat_.channels);
	ok &= FLAC__stream_encoder_set_bits_per_sample(encoder_, sampleFormat_.bits);
	ok &= FLAC__stream_encoder_set_sample_rate(encoder_, sampleFormat_.rate);

	if (!ok)
		throw SnapException("error setting up encoder");

	// now add some metadata; we'll add some tags and a padding block
	if (
			(metadata_[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) == NULL ||
			(metadata_[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) == NULL ||
			// there are many tag (vorbiscomment) functions but these are convenient for this particular use:
			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "TITLE", "SnapStream") ||
			!FLAC__metadata_object_vorbiscomment_append_comment(metadata_[0], entry, false) ||
			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "VERSION", VERSION) ||
			!FLAC__metadata_object_vorbiscomment_append_comment(metadata_[0], entry, false)
		)
		throw SnapException("out of memory or tag error");

	metadata_[1]->length = 1234; // set the padding length
	ok = FLAC__stream_encoder_set_metadata(encoder_, metadata_, 2);
	if (!ok)
		throw SnapException("error setting meta data");

	// initialize encoder
	init_status = FLAC__stream_encoder_init_stream(encoder_, ::write_callback, NULL, NULL, NULL, this);
	if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
		throw SnapException("ERROR: initializing encoder: " + string(FLAC__StreamEncoderInitStatusString[init_status]));
}

