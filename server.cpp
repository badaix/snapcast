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
    Chunk* chunk = new Chunk();
    timeval now;
    gettimeofday(&now, NULL);
	long nextTick = getTickCount();
    while (cin.good())
    {
		long currentTick = getTickCount();
		nextTick += WIRE_CHUNK_MS;
		for (size_t n=0; (n<WIRE_CHUNK_SIZE) && cin.good(); ++n)
		{
			c[0] = cin.get();
			c[1] = cin.get();
	        chunk->payload[n] = (int)c[0] + ((int)c[1] * 256);
		}

        chunk->tv_sec = now.tv_sec;
        chunk->tv_usec = now.tv_usec;
		chunk->idx = 0;
        zmq::message_t message(sizeof(Chunk));
        memcpy(message.data(), chunk, sizeof(Chunk));
        publisher.send(message);
        addMs(now, WIRE_CHUNK_MS);

		if (nextTick - currentTick > 0)
		{
			usleep((nextTick - currentTick) * 1000);
		}
    }
	delete chunk;
    return 0;
}

