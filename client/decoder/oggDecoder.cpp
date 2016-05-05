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

#include "oggDecoder.h"
#include "common/snapException.h"
#include "common/endian.h"
#include "common/log.h"


using namespace std;


OggDecoder::OggDecoder() : Decoder()
{
	ogg_sync_init(&oy); /* Now we can read pages */
}


OggDecoder::~OggDecoder()
{
	std::lock_guard<std::mutex> lock(mutex_);
	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);
	ogg_stream_clear(&os);
	vorbis_comment_clear(&vc);
	vorbis_info_clear(&vi);  /* must be called last */
	ogg_sync_clear(&oy);
}


bool OggDecoder::decode(msg::PcmChunk* chunk)
{
	std::lock_guard<std::mutex> lock(mutex_);
	/* grab some data at the head of the stream. We want the first page
	(which is guaranteed to be small and only contain the Vorbis
	stream initial header) We need the first page to get the stream
	serialno. */
	int size = chunk->payloadSize;
	char *buffer = ogg_sync_buffer(&oy, size);
	memcpy(buffer, chunk->payload, size);
	ogg_sync_wrote(&oy, size);


	chunk->payloadSize = 0;
	/* The rest is just a straight decode loop until end of stream */
	//      while(!eos){
	while(true)
	{
		int result = ogg_sync_pageout(&oy, &og);
		if (result == 0)
			break; /* need more data */
		if (result < 0)
		{
			/* missing or corrupt data at this page position */
			logE << "Corrupt or missing data in bitstream; continuing...\n";
			continue;
		}

		ogg_stream_pagein(&os,&og); /* can safely ignore errors at this point */
		while(1)
		{
			result = ogg_stream_packetout(&os, &op);

			if (result == 0)
				break; /* need more data */
			if (result < 0)
				continue; /* missing or corrupt data at this page position */
			/* no reason to complain; already complained above */
			/* we have a packet.  Decode it */
#ifdef HAS_TREMOR
			ogg_int32_t **pcm;
#else
			float **pcm;
#endif
			int samples;

			if (vorbis_synthesis(&vb,&op) == 0) /* test for success! */
				vorbis_synthesis_blockin(&vd, &vb);
			/*
			**pcm is a multichannel float vector.  In stereo, for
			example, pcm[0] is left, and pcm[1] is right.  samples is
			the size of each channel.  Convert the float values
			(-1.<=range<=1.) to whatever PCM format and write it out */
			while ((samples = vorbis_synthesis_pcmout(&vd, &pcm)) > 0)
			{
				size_t bytes = sampleFormat_.sampleSize * vi.channels * samples;
				chunk->payload = (char*)realloc(chunk->payload, chunk->payloadSize + bytes);
				for (int channel = 0; channel < vi.channels; ++channel)
				{
					if (sampleFormat_.sampleSize == 1)
					{
						int8_t* chunkBuffer = (int8_t*)(chunk->payload + chunk->payloadSize);
						for (int i = 0; i < samples; i++)
						{
							int8_t& val = chunkBuffer[sampleFormat_.channels*i + channel];
#ifdef HAS_TREMOR
							val = clip<int8_t>(pcm[channel][i], -128, 127);
#else
							val = clip<int8_t>(floor(pcm[channel][i]*127.f + .5f), -128, 127);
#endif
						}
					}
					else if (sampleFormat_.sampleSize == 2)
					{
						int16_t* chunkBuffer = (int16_t*)(chunk->payload + chunk->payloadSize);
						for (int i = 0; i < samples; i++)
						{
							int16_t& val = chunkBuffer[sampleFormat_.channels*i + channel];
#ifdef HAS_TREMOR
							val = SWAP_16(clip<int16_t>(pcm[channel][i] >> 9, -32768, 32767));
#else
							val = SWAP_16(clip<int16_t>(floor(pcm[channel][i]*32767.f + .5f), -32768, 32767));
#endif
						}
					}
					else if (sampleFormat_.sampleSize == 4)
					{
						int32_t* chunkBuffer = (int32_t*)(chunk->payload + chunk->payloadSize);
						for (int i = 0; i < samples; i++)
						{
							int32_t& val = chunkBuffer[sampleFormat_.channels*i + channel];
#ifdef HAS_TREMOR
							val = SWAP_32(clip<int32_t>(pcm[channel][i] << 7, -2147483648, 2147483647));
#else
							val = SWAP_32(clip<int32_t>(floor(pcm[channel][i]*2147483647.f + .5f), -2147483648, 2147483647));
#endif
						}
					}
				}

				chunk->payloadSize += bytes;
				vorbis_synthesis_read(&vd, samples);
			}
		}
	}

	return true;
}


SampleFormat OggDecoder::setHeader(msg::CodecHeader* chunk)
{
	int size = chunk->payloadSize;
	char *buffer = ogg_sync_buffer(&oy, size);
	memcpy(buffer, chunk->payload, size);
	ogg_sync_wrote(&oy, size);

	if (ogg_sync_pageout(&oy, &og) != 1)
		throw SnapException("Input does not appear to be an Ogg bitstream");

	ogg_stream_init(&os,ogg_page_serialno(&og));

	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);
	if (ogg_stream_pagein(&os, &og) < 0)
		throw SnapException("Error reading first page of Ogg bitstream data");

	if (ogg_stream_packetout(&os, &op) != 1)
		throw SnapException("Error reading initial header packet");

	if (vorbis_synthesis_headerin(&vi, &vc, &op) < 0)
		throw SnapException("This Ogg bitstream does not contain Vorbis audio data");

	int i(0);
	while (i < 2)
	{
		while (i < 2)
		{
			int result=ogg_sync_pageout(&oy, &og);
			if (result == 0)
				break; /* Need more data */
			/* Don't complain about missing or corrupt data yet. We'll
			   catch it at the packet output phase */
			if (result == 1)
			{
				ogg_stream_pagein(&os, &og); /* we can ignore any errors here as they'll also become apparent at packetout */
				while (i < 2)
				{
					result=ogg_stream_packetout(&os, &op);
					if (result == 0)
						break;
					/// Uh oh; data at some point was corrupted or missing!
					/// We can't tolerate that in a header.  Die. */
					if (result < 0)
						throw SnapException("Corrupt secondary header.  Exiting.");

					result=vorbis_synthesis_headerin(&vi, &vc, &op);
					if (result < 0)
						throw SnapException("Corrupt secondary header.  Exiting.");

					i++;
				}
			}
		}
	}

	/// OK, got and parsed all three headers. Initialize the Vorbis packet->PCM decoder.
	if (vorbis_synthesis_init(&vd, &vi) == 0)
		vorbis_block_init(&vd, &vb);
	/// central decode state
	/// local state for most of the decode so multiple block decodes can proceed
	/// in parallel. We could init multiple vorbis_block structures for vd here

	sampleFormat_.setFormat(vi.rate, 16, vi.channels);

	/* Throw the comments plus a few lines about the bitstream we're decoding */
	char **ptr=vc.user_comments;
	while (*ptr)
	{
		std::string comment(*ptr);
		if (comment.find("SAMPLE_FORMAT=") == 0)
			sampleFormat_.setFormat(comment.substr(comment.find("=") + 1));
		logO << "comment: " << comment << "\n";;
		++ptr;
	}

	logO << "Encoded by: " << vc.vendor << "\n";

	return sampleFormat_;
}



