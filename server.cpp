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

using namespace std;

const int size(1024);

struct Chunk
{
        long tv_sec;
        long tv_usec;
	char payload[size];
};

int main () {
    //  Prepare our context and publisher
    zmq::context_t context (1);
    zmq::socket_t publisher (context, ZMQ_PUB);
    publisher.bind("tcp://0.0.0.0:123458");

    //  Initialize random number generator
    size_t idx(0);
    char c;//[2];
	Chunk chunk;
    while (!cin.get(c).eof())
    {
        timeval ts;
		if (idx == 0)
		{
			gettimeofday(&ts, NULL);
			chunk.tv_sec = ts.tv_sec;
			chunk.tv_usec = ts.tv_usec;
                }        
//        read(fd, &msg[0], size);
        chunk.payload[idx++] = c;
        if (idx == size)
        {
            zmq::message_t message(sizeof(Chunk));
            chunk.tv_usec = 3;
            memcpy(message.data(), &chunk, sizeof(Chunk));
//            snprintf ((char *) message.data(), size, "%05d %d", zipcode, c);
//  	      message.data()[0] = c;
            publisher.send(message);
            idx = 0;
//            msg[0] = '0';
        }
    }
    return 0;
}

