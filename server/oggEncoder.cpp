/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#include "oggEncoder.h"
#include "common/log.h"
#include <iostream>
#include <cstring>

using namespace std;


OggEncoder::OggEncoder(const msg::SampleFormat& format) : Encoder(format), lastGranulepos(0), eos(0)
{
	init();
}



double OggEncoder::encode(msg::PcmChunk* chunk)
{
	double res = 0;
	logO << "payload: " << chunk->payloadSize << "\tframes: " << chunk->getFrameCount() << "\tduration: " << chunk->duration<chronos::msec>().count() << "\n";
	int bytes = chunk->payloadSize / 4;
	float **buffer=vorbis_analysis_buffer(&vd, bytes);

	/* uninterleave samples */
	for (int i=0; i<bytes; i++)
	{
		int idx = 4*i;
		buffer[0][i]=((((int8_t)chunk->payload[idx+1]) << 8) | (0x00ff&((int8_t)chunk->payload[idx])))/32768.f;
		buffer[1][i]=((((int8_t)chunk->payload[idx+3]) << 8) | (0x00ff&((int8_t)chunk->payload[idx+2])))/32768.f;
	}

	/* tell the library how much we actually submitted */
	vorbis_analysis_wrote(&vd, bytes);

	/* vorbis does some data preanalysis, then divvies up blocks for
	more involved (potentially parallel) processing.  Get a single
	block for encoding now */
	size_t pos = 0;
	while (vorbis_analysis_blockout(&vd,&vb)==1)
	{
		/* analysis, assume we want to use bitrate management */
		vorbis_analysis(&vb,NULL);
		vorbis_bitrate_addblock(&vb);

		while (vorbis_bitrate_flushpacket(&vd,&op))
		{
			/* weld the packet into the bitstream */
			ogg_stream_packetin(&os, &op);

			/* write out pages (if any) */
			while (true)
			{
				int result = ogg_stream_flush(&os, &og);
				if (result == 0)
					break;
				res = os.granulepos - lastGranulepos;

				size_t nextLen = pos + og.header_len + og.body_len;
				// make chunk larger
				if (chunk->payloadSize < nextLen)
					chunk->payload = (char*)realloc(chunk->payload, nextLen);

				memcpy(chunk->payload + pos, og.header, og.header_len);
				pos += og.header_len;
				memcpy(chunk->payload + pos, og.body, og.body_len);
				pos += og.body_len;
				
				if (ogg_page_eos(&og))
					break;
			}
		}
	}
	
	if (res > 0)
	{
		res /= (sampleFormat_.rate / 1000.);
		logO << "res: " << res << "\n";
		lastGranulepos = os.granulepos;
		// make chunk smaller
		chunk->payload = (char*)realloc(chunk->payload, pos);
		chunk->payloadSize = pos;
	}
	
	return res;
}


void OggEncoder::init()
{
	/********** Encode setup ************/
	vorbis_info_init(&vi);

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

	ret = vorbis_encode_init_vbr(&vi, sampleFormat_.channels, sampleFormat_.rate, 1.0);

	/* do not continue if setup failed; this can happen if we ask for a
	 mode that libVorbis does not support (eg, too low a bitrate, etc,
	 will return 'OV_EIMPL') */

	if (ret)
		exit(1);

	/* add a comment */
	vorbis_comment_init(&vc);
	vorbis_comment_add_tag(&vc, "TITLE", "SnapStream");
	vorbis_comment_add_tag(&vc, "VERSION", VERSION);

	/* set up the analysis state and auxiliary encoding storage */
	vorbis_analysis_init(&vd,&vi);
	vorbis_block_init(&vd,&vb);

	/* set up our packet->stream encoder */
	/* pick a random serial number; that way we can more likely build
	 chained streams just by concatenation */
	srand(time(NULL));
	ogg_stream_init(&os,rand());

	/* Vorbis streams begin with three headers; the initial header (with
	 most of the codec setup parameters) which is mandated by the Ogg
	 bitstream spec.  The second header holds any comment fields.  The
	 third header holds the bitstream codebook.  We merely need to
	 make the headers, then pass them to libvorbis one at a time;
	 libvorbis handles the additional Ogg bitstream constraints */

	ogg_packet header;
	ogg_packet header_comm;
	ogg_packet header_code;
	    
	vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code);
	ogg_stream_packetin(&os,&header);
	ogg_stream_packetin(&os,&header_comm);
	ogg_stream_packetin(&os,&header_code);

	/* This ensures the actual
	 * audio data will start on a new page, as per spec
	 */
	size_t pos(0);
	headerChunk_ = new msg::Header("ogg");
	while (true)
	{
		int result=ogg_stream_flush(&os,&og);
		if (result == 0)
			break;
		headerChunk_->payloadSize += og.header_len + og.body_len;
		headerChunk_->payload = (char*)realloc(headerChunk_->payload, headerChunk_->payloadSize);
		logD << "HeadLen: " << og.header_len << ", bodyLen: " << og.body_len << ", result: " << result << "\n";
		memcpy(headerChunk_->payload + pos, og.header, og.header_len);
		pos += og.header_len;
		memcpy(headerChunk_->payload + pos, og.body, og.body_len);
		pos += og.body_len;
	}
}


