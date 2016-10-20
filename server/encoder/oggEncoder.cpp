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

#include "oggEncoder.h"
#include "common/snapException.h"
#include "common/strCompat.h"
#include "common/utils.h"
#include "common/log.h"

using namespace std;


OggEncoder::OggEncoder(const std::string& codecOptions) : Encoder(codecOptions), lastGranulepos_(0)
{
}


std::string OggEncoder::getAvailableOptions() const
{
	return "VBR:[-0.1 - 1.0]";
}


std::string OggEncoder::getDefaultOptions() const
{
	return "VBR:0.9";
}


std::string OggEncoder::name() const
{
	return "ogg";
}


void OggEncoder::encode(const msg::PcmChunk* chunk)
{
	double res = 0;
	logD << "payload: " << chunk->payloadSize << "\tframes: " << chunk->getFrameCount() << "\tduration: " << chunk->duration<chronos::msec>().count() << "\n";
	int frames = chunk->getFrameCount();
	float **buffer=vorbis_analysis_buffer(&vd_, frames);

	/* uninterleave samples */
	for (size_t channel = 0; channel < sampleFormat_.channels; ++channel)
	{
		if (sampleFormat_.sampleSize == 1)
		{
			int8_t* chunkBuffer = (int8_t*)chunk->payload; 
			for (int i=0; i<frames; i++)
				buffer[channel][i]= chunkBuffer[sampleFormat_.channels*i + channel] / 128.f;
		}
		else if (sampleFormat_.sampleSize == 2)
		{
			int16_t* chunkBuffer = (int16_t*)chunk->payload; 
			for (int i=0; i<frames; i++)
				buffer[channel][i]= chunkBuffer[sampleFormat_.channels*i + channel] / 32768.f;
		}
		else if (sampleFormat_.sampleSize == 4)
		{
			int32_t* chunkBuffer = (int32_t*)chunk->payload; 
			for (int i=0; i<frames; i++)
				buffer[channel][i]= chunkBuffer[sampleFormat_.channels*i + channel] / 2147483648.f;
		}
	}

	/* tell the library how much we actually submitted */
	vorbis_analysis_wrote(&vd_, frames);

	msg::PcmChunk* oggChunk = new msg::PcmChunk(chunk->format, 0);

	/* vorbis does some data preanalysis, then divvies up blocks for
	more involved (potentially parallel) processing.  Get a single
	block for encoding now */
	size_t pos = 0;
	while (vorbis_analysis_blockout(&vd_, &vb_)==1)
	{
		/* analysis, assume we want to use bitrate management */
		vorbis_analysis(&vb_, NULL);
		vorbis_bitrate_addblock(&vb_);

		while (vorbis_bitrate_flushpacket(&vd_, &op_))
		{
			/* weld the packet into the bitstream */
			ogg_stream_packetin(&os_, &op_);

			/* write out pages (if any) */
			while (true)
			{
				int result = ogg_stream_flush(&os_, &og_);
				if (result == 0)
					break;
				res = os_.granulepos - lastGranulepos_;

				size_t nextLen = pos + og_.header_len + og_.body_len;
				// make chunk larger
				if (oggChunk->payloadSize < nextLen)
					oggChunk->payload = (char*)realloc(oggChunk->payload, nextLen);

				memcpy(oggChunk->payload + pos, og_.header, og_.header_len);
				pos += og_.header_len;
				memcpy(oggChunk->payload + pos, og_.body, og_.body_len);
				pos += og_.body_len;

				if (ogg_page_eos(&og_))
					break;
			}
		}
	}

	if (res > 0)
	{
		res /= (sampleFormat_.rate / 1000.);
		// logO << "res: " << res << "\n";
		lastGranulepos_ = os_.granulepos;
		// make oggChunk smaller
		oggChunk->payload = (char*)realloc(oggChunk->payload, pos);
		oggChunk->payloadSize = pos;
		listener_->onChunkEncoded(this, oggChunk, res);
	}
	else
		delete oggChunk;
}


void OggEncoder::initEncoder()
{
	if (codecOptions_.find(":") == string::npos)
		throw SnapException("Invalid codec options: \"" + codecOptions_ + "\"");
	string mode = trim_copy(codecOptions_.substr(0, codecOptions_.find(":")));
	if (mode != "VBR")
		throw SnapException("Unsupported codec mode: \"" + mode + "\". Available: \"VBR\"");

	string qual = trim_copy(codecOptions_.substr(codecOptions_.find(":") + 1));
	double quality = 1.0;
	try
	{
		quality = cpt::stod(qual);
	}
	catch(...)
	{
		throw SnapException("Invalid codec option: \"" + codecOptions_ + "\"");
	}
	if ((quality < -0.1) || (quality > 1.0))
	{
		throw SnapException("compression level has to be between -0.1 and 1.0");
	}

	/********** Encode setup ************/
	vorbis_info_init(&vi_);

	/* choose an encoding mode.  A few possibilities commented out, one
	 actually used: */

	/*********************************************************************
	Encoding using a VBR quality mode.  The usable range is -.1
	(lowest quality, smallest file) to 1. (highest quality, largest file).
	Example quality mode .4: 44kHz stereo coupled, roughly 128kbps VBR

	ret = vorbis_encode_init_vbr(&vi,2,44100,.4);

	---------------------------------------------------------------------

	Encoding using an average bitrate mode (ABR).
	example: 44kHz stereo coupled, average 128kbps VBR

	ret = vorbis_encode_init(&vi,2,44100,-1,128000,-1);

	---------------------------------------------------------------------

	Encode using a quality mode, but select that quality mode by asking for
	an approximate bitrate.  This is not ABR, it is true VBR, but selected
	using the bitrate interface, and then turning bitrate management off:

	ret = ( vorbis_encode_setup_managed(&vi,2,44100,-1,128000,-1) ||
		   vorbis_encode_ctl(&vi,OV_ECTL_RATEMANAGE2_SET,NULL) ||
		   vorbis_encode_setup_init(&vi));

	*********************************************************************/

	int ret = vorbis_encode_init_vbr(&vi_, sampleFormat_.channels, sampleFormat_.rate, quality);

	/* do not continue if setup failed; this can happen if we ask for a
	 mode that libVorbis does not support (eg, too low a bitrate, etc,
	 will return 'OV_EIMPL') */

	if (ret)
		throw SnapException("failed to init encoder");

	/* add a comment */
	vorbis_comment_init(&vc_);
	vorbis_comment_add_tag(&vc_, "TITLE", "SnapStream");
	vorbis_comment_add_tag(&vc_, "VERSION", VERSION);
	vorbis_comment_add_tag(&vc_, "SAMPLE_FORMAT", sampleFormat_.getFormat().c_str());

	/* set up the analysis state and auxiliary encoding storage */
	vorbis_analysis_init(&vd_, &vi_);
	vorbis_block_init(&vd_, &vb_);

	/* set up our packet->stream encoder */
	/* pick a random serial number; that way we can more likely build
	 chained streams just by concatenation */
	srand(time(NULL));
	ogg_stream_init(&os_, rand());

	/* Vorbis streams begin with three headers; the initial header (with
	 most of the codec setup parameters) which is mandated by the Ogg
	 bitstream spec.  The second header holds any comment fields.  The
	 third header holds the bitstream codebook.  We merely need to
	 make the headers, then pass them to libvorbis one at a time;
	 libvorbis handles the additional Ogg bitstream constraints */

	ogg_packet header;
	ogg_packet header_comm;
	ogg_packet header_code;

	vorbis_analysis_headerout(&vd_, &vc_, &header, &header_comm, &header_code);
	ogg_stream_packetin(&os_, &header);
	ogg_stream_packetin(&os_, &header_comm);
	ogg_stream_packetin(&os_, &header_code);

	/* This ensures the actual
	 * audio data will start on a new page, as per spec
	 */
	size_t pos(0);
	headerChunk_.reset(new msg::CodecHeader("ogg"));
	while (true)
	{
		int result = ogg_stream_flush(&os_, &og_);
		if (result == 0)
			break;
		headerChunk_->payloadSize += og_.header_len + og_.body_len;
		headerChunk_->payload = (char*)realloc(headerChunk_->payload, headerChunk_->payloadSize);
		logD << "HeadLen: " << og_.header_len << ", bodyLen: " << og_.body_len << ", result: " << result << "\n";
		memcpy(headerChunk_->payload + pos, og_.header, og_.header_len);
		pos += og_.header_len;
		memcpy(headerChunk_->payload + pos, og_.body, og_.body_len);
		pos += og_.body_len;
	}
}


