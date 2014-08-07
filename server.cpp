//
//  Weather update server in C++
//  Binds PUB socket to tcp://*:5556
//  Publishes random weather updates
//
//  Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>
//
#include <zmq.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdint.h>
#include "chunk.h"
#include "timeUtils.h"


using namespace std;



int main () {
    //  Prepare our context and publisher
    zmq::context_t context (1);
    zmq::socket_t publisher (context, ZMQ_PUB);
    publisher.bind("tcp://0.0.0.0:123458");

    char c[2];
    WireChunk* chunk = new WireChunk();
    timeval tvChunk;
    gettimeofday(&tvChunk, NULL);
	long nextTick = getTickCount();

    cin.sync();

    while (cin.good())
    {
		for (size_t n=0; (n<WIRE_CHUNK_SIZE) && cin.good(); ++n)
		{
			c[0] = cin.get();
			c[1] = cin.get();
	        chunk->payload[n] = (int)c[0] + ((int)c[1] << 8);
		}

//		if (!cin.good())
//			cin.clear();

        chunk->tv_sec = tvChunk.tv_sec;
        chunk->tv_usec = tvChunk.tv_usec;
        zmq::message_t message(sizeof(WireChunk));
        memcpy(message.data(), chunk, sizeof(WireChunk));
        publisher.send(message);

        addMs(tvChunk, WIRE_CHUNK_MS);
		nextTick += WIRE_CHUNK_MS;
		long currentTick = getTickCount();
		if (nextTick > currentTick)
		{
			usleep((nextTick - currentTick) * 1000);
		}
		else
		{
			cin.sync();
			gettimeofday(&tvChunk, NULL);
			nextTick = getTickCount();
		}
    }
	delete chunk;
    return 0;
}


