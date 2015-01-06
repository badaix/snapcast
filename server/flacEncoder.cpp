#include "flacEncoder.h"
#include "common/log.h"
#include <iostream>

using namespace std;

#define READSIZE 16384

static FLAC__int32 pcm[READSIZE/*samples*/ * 2/*channels*/];
size_t encodedSamples = 0;
static msg::PcmChunk* encodedChunk = NULL;


FlacEncoder::FlacEncoder(const msg::SampleFormat& format) : Encoder(format), encoder(NULL)
{
	encodedChunk = new msg::PcmChunk();
	headerChunk = new msg::Header("flac");
	initEncoder();
}


FlacEncoder::~FlacEncoder()
{
	if (encoder != NULL)
	{
		FLAC__stream_encoder_finish(encoder);
		FLAC__metadata_object_delete(metadata[0]);
		FLAC__metadata_object_delete(metadata[1]);
		FLAC__stream_encoder_delete(encoder);
	}

	delete encodedChunk;
}



msg::Header* FlacEncoder::getHeaderChunk()
{
	return headerChunk;
}


double FlacEncoder::encode(msg::PcmChunk* chunk)
{
logD << "payload: " << chunk->payloadSize << "\tsamples: " << chunk->payloadSize/4 << "\n";
	int samples = chunk->payloadSize / 4;
	for(int i=0; i<samples*2/*channels*/; i++)
	{
		pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)chunk->payload[2*i+1] << 8) | (FLAC__int16)(0x00ff&chunk->payload[2*i]));
	}
	FLAC__stream_encoder_process_interleaved(encoder, pcm, samples);

	double res = encodedSamples / ((double)sampleFormat.rate / 1000.);
	if (encodedSamples > 0)
	{
logD << "encoded: " << chunk->payloadSize << "\tsamples: " << encodedSamples << "\tres: " << res << "\n";
		encodedSamples = 0;
		chunk->payloadSize = encodedChunk->payloadSize;
		chunk->payload = (char*)realloc(chunk->payload, encodedChunk->payloadSize);
		memcpy(chunk->payload, encodedChunk->payload, encodedChunk->payloadSize);
		encodedChunk->payloadSize = 0;
		encodedChunk->payload = (char*)realloc(encodedChunk->payload, 0);
	}

	return res;//chunk->duration<chronos::msec>().count();
}


FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder,
    const FLAC__byte buffer[],
    size_t bytes,
    unsigned samples,
    unsigned current_frame,
    void *client_data)
{
logD << "write_callback: " << bytes << ", " << samples << ", " << current_frame << "\n";
	FlacEncoder* flacEncoder = (FlacEncoder*)client_data;
	if ((current_frame == 0) && (bytes > 0) && (samples == 0))
	{
		msg::Header* headerChunk = flacEncoder->getHeaderChunk();
		headerChunk->payload = (char*)realloc(headerChunk->payload, headerChunk->payloadSize + bytes);
		memcpy(headerChunk->payload + headerChunk->payloadSize, buffer, bytes);
		headerChunk->payloadSize += bytes;
	}
	else
	{
		encodedChunk->payload = (char*)realloc(encodedChunk->payload, encodedChunk->payloadSize + bytes);
		memcpy(encodedChunk->payload + encodedChunk->payloadSize, buffer, bytes);
		encodedChunk->payloadSize += bytes;
		encodedSamples += samples;
	}
	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}





void FlacEncoder::initEncoder()
{
	FLAC__bool ok = true;
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata_VorbisComment_Entry entry;

	// allocate the encoder
	if((encoder = FLAC__stream_encoder_new()) == NULL) {
		fprintf(stderr, "ERROR: allocating encoder\n");
		return;
	}

	ok &= FLAC__stream_encoder_set_verify(encoder, true);
	ok &= FLAC__stream_encoder_set_compression_level(encoder, 5);
	ok &= FLAC__stream_encoder_set_channels(encoder, sampleFormat.channels);
	ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, sampleFormat.bits);
	ok &= FLAC__stream_encoder_set_sample_rate(encoder, sampleFormat.rate);

	// now add some metadata; we'll add some tags and a padding block
	if(ok) {
		if(
			(metadata[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT)) == NULL ||
			(metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) == NULL ||
			// there are many tag (vorbiscomment) functions but these are convenient for this particular use:
			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ARTIST", "Some Artist") ||
			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, false) || 
			!FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "YEAR", "1984") ||
			!FLAC__metadata_object_vorbiscomment_append_comment(metadata[0], entry, false)
		) {
			fprintf(stderr, "ERROR: out of memory or tag error\n");
			ok = false;
		}

		metadata[1]->length = 1234; // set the padding length

		ok = FLAC__stream_encoder_set_metadata(encoder, metadata, 2);
	}

	// initialize encoder
	if(ok) {
		init_status = FLAC__stream_encoder_init_stream(encoder, write_callback, NULL, NULL, NULL, this);
		if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
			fprintf(stderr, "ERROR: initializing encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
			ok = false;
		}
	}
/*
	// read blocks of samples from WAVE file and feed to encoder
	if(ok) {
		size_t left = (size_t)total_samples;
		while(ok && left) {
			size_t need = (left>READSIZE? (size_t)READSIZE : (size_t)left);
			if(fread(buffer, channels*(bps/8), need, fin) != need) {
				fprintf(stderr, "ERROR: reading from WAVE file\n");
				ok = false;
			}
			else {
				// convert the packed little-endian 16-bit PCM samples from WAVE into an interleaved FLAC__int32 buffer for libFLAC
				size_t i;
				for(i = 0; i < need*channels; i++) {
					// inefficient but simple and works on big- or little-endian machines
					pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)buffer[2*i+1] << 8) | (FLAC__int16)buffer[2*i]);
				}
				// feed samples to encoder
				ok = FLAC__stream_encoder_process_interleaved(encoder, pcm, need);
			}
			left -= need;
		}
	}

	ok &= FLAC__stream_encoder_finish(encoder);

	fprintf(stderr, "encoding: %s\n", ok? "succeeded" : "FAILED");
	fprintf(stderr, "   state: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);

	// now that encoding is finished, the metadata can be freed
	FLAC__metadata_object_delete(metadata[0]);
	FLAC__metadata_object_delete(metadata[1]);

	FLAC__stream_encoder_delete(encoder);
	fclose(fin);

	return 0;
*/
}

/*
void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data)
{
	(void)encoder, (void)client_data;
	fprintf(stderr, "wrote %d bytes, %d, %u samples, %u/%u frames\n", bytes_written, samples_written, total_samples, frames_written, total_frames_estimate);
}
*/




