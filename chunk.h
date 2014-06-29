#ifndef CHUNK_H
#define CHUNK_H

#define MS (40)
//44100 / 20 = 2205
#define SAMPLE_RATE (44100)
#define SIZE (SAMPLE_RATE*4*MS/1000)
int bufferMs;
#define FRAMES_PER_BUFFER  (SIZE/4)


struct Chunk
{
	int32_t tv_sec;
	int32_t tv_usec;
	char payload[SIZE];
};

#endif

