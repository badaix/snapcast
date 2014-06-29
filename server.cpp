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

    //  Initialize random number generator
    size_t idx(0);
    char c;//[2];
    Chunk chunk;
    timeval ts;
    ts.tv_sec = 0;
    ts.tv_usec = 0;
    timeval last;
    gettimeofday(&last, NULL);
    last.tv_sec -= 1000;
    while (!cin.get(c).eof())
    {
//        read(fd, &msg[0], size);
        chunk.payload[idx++] = c;
        if (idx == WIRE_CHUNK_SIZE)
        {
            timeval now;
            gettimeofday(&now, NULL);
            if (diff_ms(now, last) > 200)
                ts = now;
            last = now;
                
//            if (ts.tv_sec == 0)
//                ts = now;
//            else if (diff_ms(now, ts) > 1000)
//                ts = now;

            chunk.tv_sec = ts.tv_sec;
            chunk.tv_usec = ts.tv_usec;
            zmq::message_t message(sizeof(Chunk));
            memcpy(message.data(), &chunk, sizeof(Chunk));
//            snprintf ((char *) message.data(), size, "%05d %d", zipcode, c);
//  	      message.data()[0] = c;
            publisher.send(message);
            addMs(ts, WIRE_CHUNK_MS);
            idx = 0;
//            msg[0] = '0';
        }
    }
    return 0;
}

