#include "chunk.h"
#include <string.h>
#include <iostream>


Chunk::Chunk(WireChunk* _wireChunk) : idx(0)
{
	wireChunk = new WireChunk(*_wireChunk);
}


Chunk::~Chunk()
{
	delete wireChunk;
}


bool Chunk::isEndOfChunk()
{
	return idx >= WIRE_CHUNK_SIZE;
}


bool Chunk::getNext(int16_t& _result)
{
	if (isEndOfChunk())
		return false;
	_result = wireChunk->payload[idx++];
	result = true;
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


