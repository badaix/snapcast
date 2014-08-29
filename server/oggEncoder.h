#ifndef OGG_ENCODER_H
#define OGG_ENCODER_H
#include "encoder.h"
#include <vorbis/vorbisenc.h>


class OggEncoder
{
public:
	OggEncoder();
	virtual bool encode(Chunk* chunk);

private:
	void init();

	ogg_stream_state os; /* take physical pages, weld into a logical
		                  stream of packets */
	ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
	ogg_packet       op; /* one raw packet of data for decode */

	vorbis_info      vi; /* struct that stores all the static vorbis bitstream
		                  settings */
	vorbis_comment   vc; /* struct that stores all the user comments */

	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block     vb; /* local working space for packet->PCM decode */

	ogg_packet header;
	ogg_packet header_comm;
	ogg_packet header_code;

	int eos=0,ret;
	int i, founddata;

	int32_t tv_sec;
	int32_t tv_usec;
};


#endif


