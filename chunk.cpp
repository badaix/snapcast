#include "chunk.h"
#include <string.h>


Chunk::Chunk(WireChunk* _wireChunk) : idx(0), wireChunk(_wireChunk)
{
}


Chunk::~Chunk()
{
	delete wireChunk;
}


bool Chunk::isEndOfChunk()
{
	return idx >= WIRE_CHUNK_SIZE;
}


int Chunk::read(short* _outputBuffer, int _count)
{
	int result = _count;
	if (idx + _count > WIRE_CHUNK_SIZE)
		result = WIRE_CHUNK_SIZE - idx;

	if (_outputBuffer != NULL)
		memcpy(_outputBuffer, &wireChunk->payload[idx], sizeof(int16_t)*result);

	idx += result;
	return result;
}


