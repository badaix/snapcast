#ifndef CHUNK_H
#define CHUNK_H

#define SAMPLE_RATE (48000)
//#define SAMPLE_BIT (16)
#define CHANNELS (2)

#define WIRE_CHUNK_MS (50)
#define WIRE_CHUNK_SIZE (SAMPLE_RATE*CHANNELS*WIRE_CHUNK_MS/1000)

#define PLAYER_CHUNK_MS (10)
#define PLAYER_CHUNK_SIZE (SAMPLE_RATE*CHANNELS*PLAYER_CHUNK_MS/1000)
#define PLAYER_CHUNK_MS_SIZE (SAMPLE_RATE*CHANNELS/1000)
#define FRAMES_PER_BUFFER  (SAMPLE_RATE*PLAYER_CHUNK_MS/1000)


template <size_t T>
struct ChunkT
{
//	ChunkT()
//	{
//		memset(&payload[0], 0, T);
//	}
	int32_t tv_sec;
	int32_t tv_usec;
	int32_t idx;
	int16_t payload[T];
};


typedef ChunkT<WIRE_CHUNK_SIZE> Chunk;
typedef ChunkT<PLAYER_CHUNK_SIZE> PlayerChunk;

#endif

