#ifndef CHUNK_H
#define CHUNK_H

#define SAMPLE_RATE (48000)
#define SAMPLE_BIT (16)
#define CHANNELS (2)

#define WIRE_CHUNK_MS (40)
#define WIRE_CHUNK_SIZE (SAMPLE_RATE*CHANNELS*SAMPLE_BIT/8*WIRE_CHUNK_MS/1000)

#define PLAYER_CHUNK_MS (10)
#define PLAYER_CHUNK_SIZE (SAMPLE_RATE*CHANNELS*SAMPLE_BIT/8*PLAYER_CHUNK_MS/1000)

#define FRAMES_PER_BUFFER  (PLAYER_CHUNK_SIZE/(CHANNELS*SAMPLE_BIT/8))

int bufferMs;

template <size_t T>
struct ChunkT
{
	int32_t tv_sec;
	int32_t tv_usec;
	char payload[T];
};


typedef ChunkT<WIRE_CHUNK_SIZE> Chunk;
typedef ChunkT<PLAYER_CHUNK_SIZE> PlayerChunk;

#endif

