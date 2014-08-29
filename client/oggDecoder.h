#ifndef OGG_DECODER_H
#define OGG_DECODER_H
#include "decoder.h"
#include <vorbis/codec.h>


class OggDecoder
{
public:
	OggDecoder();
	virtual bool decode(Chunk* chunk);

private:
	void init();

	ogg_sync_state   oy; /* sync and verify incoming physical bitstream */
	ogg_stream_state os; /* take physical pages, weld into a logical
		                  stream of packets */
	ogg_page         og; /* one Ogg bitstream page. Vorbis packets are inside */
	ogg_packet       op; /* one raw packet of data for decode */

	vorbis_info      vi; /* struct that stores all the static vorbis bitstream
		                  settings */
	vorbis_comment   vc; /* struct that stores all the bitstream user comments */
	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block     vb; /* local working space for packet->PCM decode */
	ogg_int16_t convbuffer[4096]; /* take 8k out of the data segment, not the stack */
	int convsize=4096;

bool first;
	char *buffer;
	int  bytes;
};


#endif


