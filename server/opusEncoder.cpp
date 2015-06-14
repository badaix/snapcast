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

#include "opusEncoder.h"
#include "common/log.h"


OpusEncoderWrapper::OpusEncoderWrapper(const msg::SampleFormat& format) : Encoder(format)
{
	int error;
	headerChunk = new msg::Header("opus");
	enc = opus_encoder_create(format.rate, format.channels, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error);
	if(error != 0) {
		logE << "Failed to initialize opus encoder: " << error << '\n';
		logE << " Rate:     " << format.rate << '\n';
		logE << " Channels: " << format.channels << '\n';
	}
}


double OpusEncoderWrapper::encode(msg::PcmChunk* chunk)
{
	int samples = chunk->payloadSize/4;
	opus_int16 pcm[samples*2];

	unsigned char encoded[chunk->payloadSize];

	// get 16 bit samples
	for(int i=0; i<samples*2; i++)
	{
		pcm[i] = (opus_int16)(((opus_int16)chunk->payload[2*i+1] << 8) | (0x00ff & (opus_int16)chunk->payload[2*i]));
	}

	// encode
	int len = opus_encode(enc, pcm, samples, encoded, chunk->payloadSize);
	// logD << "Encoded: size " << chunk->payloadSize << " bytes, encoded: " << len << " bytes" << '\n';

	if(len>0) {
		// copy encoded data to chunk
		chunk->payloadSize = len;
		chunk->payload = (char*)realloc(chunk->payload, chunk->payloadSize);
		memcpy(chunk->payload, encoded, len);
	}
	else {
		logE << "Failed to encode chunk: " << len << '\n';
		logE << " Frame size: " << samples/2 << '\n';
		logE << " Max bytes:  " << chunk->payloadSize << '\n';
	}

	// return chunk duration
	return (double)samples / ((double)sampleFormat.rate / 1000.);
}
