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

#include "oggDecoder.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <vorbis/vorbisenc.h>


using namespace std;


OggDecoder::OggDecoder() : Decoder(), buffer(NULL)
{
	ogg_sync_init(&oy); /* Now we can read pages */
	convsize = 4096;
	convbuffer = (ogg_int16_t*)malloc(convsize * sizeof(ogg_int16_t));
}


OggDecoder::~OggDecoder()
{
	free(convbuffer);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
	ogg_stream_clear(&os);
	vorbis_comment_clear(&vc);
	vorbis_info_clear(&vi);  /* must be called last */
	ogg_sync_clear(&oy);
}


bool OggDecoder::decode(msg::PcmChunk* chunk)
{

	/* grab some data at the head of the stream. We want the first page
	(which is guaranteed to be small and only contain the Vorbis
	stream initial header) We need the first page to get the stream
	serialno. */
	bytes = chunk->payloadSize;
	buffer=ogg_sync_buffer(&oy, bytes);
	memcpy(buffer, chunk->payload, bytes);
	ogg_sync_wrote(&oy,bytes);


	chunk->payloadSize = 0;
	convsize=4096;//bytes/vi.channels;
	/* The rest is just a straight decode loop until end of stream */
	//      while(!eos){
	while(true)
	{
		int result=ogg_sync_pageout(&oy,&og);
		if (result==0)
			break; /* need more data */
		if(result<0)
		{
			/* missing or corrupt data at this page position */
			fprintf(stderr,"Corrupt or missing data in bitstream; continuing...\n");
			continue;
		}

		ogg_stream_pagein(&os,&og); /* can safely ignore errors at
					   this point */
		while(1)
		{
			result=ogg_stream_packetout(&os,&op);

			if(result==0)
				break; /* need more data */
			if(result<0)
				continue; /* missing or corrupt data at this page position */
			/* no reason to complain; already complained above */
			/* we have a packet.  Decode it */
			float **pcm;
			int samples;

			if(vorbis_synthesis(&vb,&op)==0) /* test for success! */
				vorbis_synthesis_blockin(&vd,&vb);
			/*

			**pcm is a multichannel float vector.  In stereo, for
			example, pcm[0] is left, and pcm[1] is right.  samples is
			the size of each channel.  Convert the float values
			(-1.<=range<=1.) to whatever PCM format and write it out */

			while((samples=vorbis_synthesis_pcmout(&vd,&pcm))>0)
			{
				int bout=(samples<convsize?samples:convsize);
				//cout << "samples: " << samples << ", convsize: " << convsize << "\n";
				/* convert floats to 16 bit signed ints (host order) and
				interleave */
				for(int i=0; i<vi.channels; i++)
				{
					ogg_int16_t *ptr=convbuffer+i;
					float  *mono=pcm[i];
					for(int j=0; j<bout; j++)
					{
						int val=floor(mono[j]*32767.f+.5f);
						/* might as well guard against clipping */
						if(val>32767)
							val=32767;
						else if(val<-32768)
							val=-32768;
						*ptr=val;
						ptr+=vi.channels;
					}
				}

				size_t oldSize = chunk->payloadSize;
				size_t size = 2*vi.channels * bout;
				chunk->payloadSize += size;
				chunk->payload = (char*)realloc(chunk->payload, chunk->payloadSize);
				memcpy(chunk->payload + oldSize, convbuffer, size);
				/* tell libvorbis how many samples we actually consumed */
				vorbis_synthesis_read(&vd,bout);
			}
		}
	}
	//            if(ogg_page_eos(&og))eos=1;
	//    ogg_stream_clear(&os);
	//    vorbis_comment_clear(&vc);
	//    vorbis_info_clear(&vi);  /* must be called last */
	return true;
}


bool OggDecoder::setHeader(msg::Header* chunk)
{
	bytes = chunk->payloadSize;
	buffer=ogg_sync_buffer(&oy, bytes);
	memcpy(buffer, chunk->payload, bytes);
	ogg_sync_wrote(&oy,bytes);

	if(ogg_sync_pageout(&oy,&og)!=1)
	{
		fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
		return false;
	}

	ogg_stream_init(&os,ogg_page_serialno(&og));

	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);
	if(ogg_stream_pagein(&os,&og)<0)
	{
		fprintf(stderr,"Error reading first page of Ogg bitstream data.\n");
		return false;
	}

	if(ogg_stream_packetout(&os,&op)!=1)
	{
		fprintf(stderr,"Error reading initial header packet.\n");
		return false;
	}

	if(vorbis_synthesis_headerin(&vi,&vc,&op)<0)
	{
		fprintf(stderr,"This Ogg bitstream does not contain Vorbis audio data.\n");
		return false;
	}


	int i(0);
	while(i<2)
	{
		while(i<2)
		{
			int result=ogg_sync_pageout(&oy,&og);
			if(result==0)
				break; /* Need more data */
			/* Don't complain about missing or corrupt data yet. We'll
			   catch it at the packet output phase */
			if(result==1)
			{
				ogg_stream_pagein(&os,&og); /* we can ignore any errors here as they'll also become apparent at packetout */
				while(i<2)
				{
					result=ogg_stream_packetout(&os,&op);
					if(result==0)
						break;
					if(result<0)
					{
						/* Uh oh; data at some point was corrupted or missing!
						 We can't tolerate that in a header.  Die. */
						fprintf(stderr,"Corrupt secondary header.  Exiting.\n");
						return false;
					}
					result=vorbis_synthesis_headerin(&vi,&vc,&op);
					if(result<0)
					{
						fprintf(stderr,"Corrupt secondary header.  Exiting.\n");
						return false;
					}
					i++;
				}
			}
		}
	}

	/* Throw the comments plus a few lines about the bitstream we're decoding */
	char **ptr=vc.user_comments;
	while(*ptr)
	{
		fprintf(stderr,"%s\n",*ptr);
		++ptr;
	}
	fprintf(stderr,"\nBitstream is %d channel, %ldHz\n",vi.channels,vi.rate);
	fprintf(stderr,"Encoded by: %s\n\n",vc.vendor);

	/* OK, got and parsed all three headers. Initialize the Vorbis
	   packet->PCM decoder. */
	if(vorbis_synthesis_init(&vd,&vi)==0)  /* central decode state */
		vorbis_block_init(&vd,&vb);          /* local state for most of the decode
                                              so multiple block decodes can
                                              proceed in parallel. We could init
                                              multiple vorbis_block structures
                                              for vd here */
	return false;
}



