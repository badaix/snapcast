#ifndef OGG_ENCODER_H
#define OGG_ENCODER_H
#include "encoder.h"
#include <vorbis/vorbisenc.h>


class OggEncoder : public Encoder
{
public:
	OggEncoder(const msg::SampleFormat& format);
	virtual double encode(msg::PcmChunk* chunk);

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

	ogg_int64_t   lastGranulepos;

	int eos, ret;
	int i, founddata;
};


#endif


