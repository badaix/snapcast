#include "oggDecoder.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <vorbis/vorbisenc.h>


using namespace std;


OggDecoder::OggDecoder()
{
	ogg_sync_init(&oy); /* Now we can read pages */
	convsize = 4096;
	convbuffer = (ogg_int16_t*)malloc(convsize * sizeof(ogg_int16_t));
}


OggDecoder::~OggDecoder()
{
	ogg_sync_init(&oy); /* Now we can read pages */
	delete convbuffer;
}


bool OggDecoder::decodePayload(PcmChunk* chunk)
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
		{ /* missing or corrupt data at this page position */
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
				for(int i=0;i<vi.channels;i++)
				{
					ogg_int16_t *ptr=convbuffer+i;
					float  *mono=pcm[i];
					for(int j=0;j<bout;j++)
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


bool OggDecoder::decodeHeader(HeaderMessage* chunk)
{
	bytes = chunk->payloadSize;
    buffer=ogg_sync_buffer(&oy, bytes);
    memcpy(buffer, chunk->payload, bytes);
    ogg_sync_wrote(&oy,bytes);

	cout << "Decode header\n";
    if(ogg_sync_pageout(&oy,&og)!=1)
	{
		fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
		return false;
    }
  
    ogg_stream_init(&os,ogg_page_serialno(&og));
cout << "2" << endl;
        
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
cout << "3" << endl;
    
    
    int i(0);
    while(i<2)
	{
		while(i<2)
		{
			int result=ogg_sync_pageout(&oy,&og);
cout << result << endl;
		    if(result==0)
				break; /* Need more data */
		    /* Don't complain about missing or corrupt data yet. We'll
		       catch it at the packet output phase */
			if(result==1)
			{
	cout << "a" << endl;                  
				ogg_stream_pagein(&os,&og); /* we can ignore any errors here as they'll also become apparent at packetout */
				while(i<2)
				{
	cout << "b" << endl;
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
      /* no harm in not checking before adding more */
//      buffer=ogg_sync_buffer(&oy,wireChunk->length);
//      bytes=fread(buffer,1,4096,stdin);
//      if(bytes==0 && i<2){
//        fprintf(stderr,"End of file before finding all Vorbis headers!\n");
//        exit(1);
//      }
//      ogg_sync_wrote(&oy,bytes);
    }
    
    /* Throw the comments plus a few lines about the bitstream we're decoding */
cout << "5" << endl;                  
      char **ptr=vc.user_comments;
	while(*ptr)
	{
		fprintf(stderr,"%s\n",*ptr);
		++ptr;
	}
	fprintf(stderr,"\nBitstream is %d channel, %ldHz\n",vi.channels,vi.rate);
	fprintf(stderr,"Encoded by: %s\n\n",vc.vendor);
    
cout << "6" << endl;                  

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


bool OggDecoder::decode(BaseMessage* chunk)
{
	if (chunk->getType() == chunk_type::payload)
		return decodePayload(chunk);
	else if (chunk->getType() == chunk_type::header)
		return decodeHeader(chunk);
	return false;
}



