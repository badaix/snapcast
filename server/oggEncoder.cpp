#include "oggEncoder.h"
#include <iostream>
#include <cstring>

using namespace std;


OggEncoder::OggEncoder()
{
	init();
}


bool OggEncoder::encode(Chunk* chunk)
{
	bool res = false;
	WireChunk* wireChunk = chunk->wireChunk;
	if (tv_sec == 0)
	{
		tv_sec = wireChunk->tv_sec;
		tv_usec = wireChunk->tv_usec;
	}
//cout << "pcm: " << wireChunk->length << endl;

	int bytes = wireChunk->length / 4;
	float **buffer=vorbis_analysis_buffer(&vd, bytes);

	int i;
	/* uninterleave samples */
	for(i=0;i<bytes;i++)
	{
		buffer[0][i]=((wireChunk->payload[i*4+1]<<8)|
				  (0x00ff&(int)wireChunk->payload[i*4]))/32768.f;
		buffer[1][i]=((wireChunk->payload[i*4+3]<<8)|
				  (0x00ff&(int)wireChunk->payload[i*4+2]))/32768.f;
	}

	/* tell the library how much we actually submitted */
	vorbis_analysis_wrote(&vd,i);

	/* vorbis does some data preanalysis, then divvies up blocks for
	more involved (potentially parallel) processing.  Get a single
	block for encoding now */
	size_t pos = 0;
	while(vorbis_analysis_blockout(&vd,&vb)==1)
	{

		/* analysis, assume we want to use bitrate management */
		vorbis_analysis(&vb,NULL);
		vorbis_bitrate_addblock(&vb);

		while(vorbis_bitrate_flushpacket(&vd,&op))
		{
			/* weld the packet into the bitstream */
			ogg_stream_packetin(&os,&op);

			/* write out pages (if any) */
			while(true)
			{
//				int result = ogg_stream_pageout(&os,&og);
				int result = ogg_stream_flush(&os,&og);
//cout << "result: " << result << "\n";
				if (result == 0)
				{
					break;
				}
				else
				{
					res = true;
					cout << "pcm: " << wireChunk->length << ", header len: " << og.header_len << ", body len: " << og.body_len << endl;
//					fwrite(og.header,1,og.header_len,stdout);
//					fwrite(og.body,1,og.body_len,stdout);
					memcpy(wireChunk->payload + pos, og.header, og.header_len);
					pos += og.header_len;
					memcpy(wireChunk->payload + pos, og.body, og.body_len);
					pos += og.body_len;
				}
			}
		}
	}
	if (res)
	{
		wireChunk->payload = (char*)realloc(wireChunk->payload, pos);
		wireChunk->length = pos;
		wireChunk->tv_sec = tv_sec;
		wireChunk->tv_usec = tv_usec;
		tv_sec = 0;
		tv_usec = 0;
	}
	return res;
}


void OggEncoder::init()
{
	/********** Encode setup ************/
	tv_sec = 0;
	tv_usec = 0;

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

	ret=vorbis_encode_init_vbr(&vi,2,48000,0.4);

	/* do not continue if setup failed; this can happen if we ask for a
	 mode that libVorbis does not support (eg, too low a bitrate, etc,
	 will return 'OV_EIMPL') */

	if(ret)exit(1);

	/* add a comment */
	vorbis_comment_init(&vc);
	vorbis_comment_add_tag(&vc,"ENCODER","encoder_example.c");

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

	vorbis_analysis_headerout(&vd,&vc,&header,&header_comm,&header_code);
	ogg_stream_packetin(&os,&header);
	ogg_stream_packetin(&os,&header_comm);
	ogg_stream_packetin(&os,&header_code);

	/* This ensures the actual
	 * audio data will start on a new page, as per spec
	 */
	/*    while(!eos){
	  int result=ogg_stream_flush(&os,&og);
	  if(result==0)break;
	  fwrite(og.header,1,og.header_len,stdout);
	  fwrite(og.body,1,og.body_len,stdout);
	}
	*/
}



