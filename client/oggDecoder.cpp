#include "oggDecoder.h"
#include <iostream>
#include <cstring>
#include <cmath>

using namespace std;


OggDecoder::OggDecoder()
{
	init();
}


bool OggDecoder::decode(Chunk* chunk)
{
	WireChunk* wireChunk = chunk->wireChunk;
    int eos=0;
    int i;

    /* grab some data at the head of the stream. We want the first page
       (which is guaranteed to be small and only contain the Vorbis
       stream initial header) We need the first page to get the stream
       serialno. */

    buffer=ogg_sync_buffer(&oy,wireChunk->length);
    memcpy(buffer, wireChunk->payload, wireChunk->length);
	bytes = wireChunk->length;
    ogg_sync_wrote(&oy,bytes);

if (first)
{
cout << "1" << endl;

    if(ogg_sync_pageout(&oy,&og)!=1){
      fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
      exit(1);
    }
  
    ogg_stream_init(&os,ogg_page_serialno(&og));
cout << "2" << endl;
        
    vorbis_info_init(&vi);
    vorbis_comment_init(&vc);
    if(ogg_stream_pagein(&os,&og)<0){ 
      fprintf(stderr,"Error reading first page of Ogg bitstream data.\n");
      exit(1);
    }
    
    if(ogg_stream_packetout(&os,&op)!=1){ 
      fprintf(stderr,"Error reading initial header packet.\n");
      exit(1);
    }
    
    if(vorbis_synthesis_headerin(&vi,&vc,&op)<0){ 
      fprintf(stderr,"This Ogg bitstream does not contain Vorbis "
              "audio data.\n");
      exit(1);
    }
cout << "3" << endl;
    
    
    i=0;
    while(i<2){
      while(i<2){
        int result=ogg_sync_pageout(&oy,&og);
cout << result << endl;
        if(result==0)break; /* Need more data */
        /* Don't complain about missing or corrupt data yet. We'll
           catch it at the packet output phase */
        if(result==1){
cout << "a" << endl;                  
          ogg_stream_pagein(&os,&og); /* we can ignore any errors here
                                         as they'll also become apparent
                                         at packetout */
          while(i<2){
cout << "b" << endl;
            result=ogg_stream_packetout(&os,&op);
            if(result==0)break;
            if(result<0){
              /* Uh oh; data at some point was corrupted or missing!
                 We can't tolerate that in a header.  Die. */
              fprintf(stderr,"Corrupt secondary header.  Exiting.\n");
              exit(1);
            }
            result=vorbis_synthesis_headerin(&vi,&vc,&op);
            if(result<0){
              fprintf(stderr,"Corrupt secondary header.  Exiting.\n");
              exit(1);
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
    
    /* Throw the comments plus a few lines about the bitstream we're
       decoding */
    {
      char **ptr=vc.user_comments;
      while(*ptr){
        fprintf(stderr,"%s\n",*ptr);
        ++ptr;
      }
      fprintf(stderr,"\nBitstream is %d channel, %ldHz\n",vi.channels,vi.rate);
      fprintf(stderr,"Encoded by: %s\n\n",vc.vendor);
    }
    

    /* OK, got and parsed all three headers. Initialize the Vorbis
       packet->PCM decoder. */
    if(vorbis_synthesis_init(&vd,&vi)==0){ /* central decode state */
      vorbis_block_init(&vd,&vb);          /* local state for most of the decode
                                              so multiple block decodes can
                                              proceed in parallel. We could init
                                              multiple vorbis_block structures
                                              for vd here */
	}
first = false;
}      
    convsize=wireChunk->length/vi.channels;
      /* The rest is just a straight decode loop until end of stream */
      while(!eos){
        while(!eos){
          int result=ogg_sync_pageout(&oy,&og);
          if(result==0)break; /* need more data */
          if(result<0){ /* missing or corrupt data at this page position */
            fprintf(stderr,"Corrupt or missing data in bitstream; "
                    "continuing...\n");
          }else{
            ogg_stream_pagein(&os,&og); /* can safely ignore errors at
                                           this point */
            while(1){
              result=ogg_stream_packetout(&os,&op);
              
              if(result==0)break; /* need more data */
              if(result<0){ /* missing or corrupt data at this page position */
                /* no reason to complain; already complained above */
              }else{
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
                
                wireChunk->length = 0;
                while((samples=vorbis_synthesis_pcmout(&vd,&pcm))>0){
                  int j;
                  int clipflag=0;
                  int bout=(samples<convsize?samples:convsize);
                  
                  /* convert floats to 16 bit signed ints (host order) and
                     interleave */
                  for(i=0;i<vi.channels;i++){
                    ogg_int16_t *ptr=convbuffer+i;
                    float  *mono=pcm[i];
                    for(j=0;j<bout;j++){
#if 1
                      int val=floor(mono[j]*32767.f+.5f);
#else /* optional dither */
                      int val=mono[j]*32767.f+drand48()-0.5f;
#endif
                      /* might as well guard against clipping */
                      if(val>32767){
                        val=32767;
                        clipflag=1;
                      }
                      if(val<-32768){
                        val=-32768;
                        clipflag=1;
                      }
                      *ptr=val;
                      ptr+=vi.channels;
                    }
                  }
                  
                  if(clipflag)
                    fprintf(stderr,"Clipping in frame %ld\n",(long)(vd.sequence));
//cout << "a" << endl;                  
size_t oldSize = wireChunk->length;
size_t size = 2*vi.channels * bout;
                  wireChunk->length += size;
                  wireChunk->payload = (char*)realloc(wireChunk->payload, wireChunk->length);
					memcpy(wireChunk->payload + oldSize, convbuffer, size);
//                  fwrite(convbuffer,2*vi.channels,bout,stdout);
                  
                  vorbis_synthesis_read(&vd,bout); /* tell libvorbis how
                                                      many samples we
                                                      actually consumed */
                }            
              }
            }
eos = 1;
//            if(ogg_page_eos(&og))eos=1;
          }
        }
//        if(!eos){
//          buffer=ogg_sync_buffer(&oy,4096);
//          bytes=fread(buffer,1,4096,stdin);
//          ogg_sync_wrote(&oy,bytes);
//         if(bytes==0)eos=1;
//        }
      }
      
      /* ogg_page and ogg_packet structs always point to storage in
         libvorbis.  They're never freed or manipulated directly */
      
//      vorbis_block_clear(&vb);
//      vorbis_dsp_clear(&vd);
//    }else{
//      fprintf(stderr,"Error: Corrupt header during playback initialization.\n");
//    }

    /* clean up this logical bitstream; before exit we see if we're
       followed by another [chained] */
    
//    ogg_stream_clear(&os);
//    vorbis_comment_clear(&vc);
//    vorbis_info_clear(&vi);  /* must be called last */
	return true;
}


void OggDecoder::init()
{
	ogg_sync_init(&oy); /* Now we can read pages */
first = true;
}



