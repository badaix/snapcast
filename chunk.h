#ifndef CHUNK_H
#define CHUNK_H

#define SAMPLE_RATE (44100)
#define SAMPLE_BIT (16)
#define CHANNELS (2)

#define CHUNK_MS (40)
//44100 / 20 = 2205
#define CHUNK_SIZE (SAMPLE_RATE*CHANNELS*SAMPLE_BIT/8*CHUNK_MS/1000)
#define FRAMES_PER_BUFFER  (CHUNK_SIZE/4)

int bufferMs;

struct Chunk
{
	int32_t tv_sec;
	int32_t tv_usec;
	char payload[CHUNK_SIZE];
};

#endif

