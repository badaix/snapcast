//
//  Weather update client in C++
//  Connects SUB socket to tcp://localhost:5556
//  Collects weather updates and finds avg temp in zipcode
//
//  Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>
//
#include <zmq.hpp>
#include <iostream>
#include <sstream>
#include <sys/time.h>

const size_t size(1764);

struct Chunk
{
	int32_t tv_sec;
	int32_t tv_usec;
	char payload[size];
};


std::string timeToStr(const timeval& timestamp)
{
	char tmbuf[64], buf[64];
	struct tm *nowtm;
	time_t nowtime;
	nowtime = timestamp.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
	snprintf(buf, sizeof buf, "%s.%06d", tmbuf, timestamp.tv_usec);
	return buf;
}


int diff_ms(const timeval& t1, const timeval& t2)
{
    return (((t1.tv_sec - t2.tv_sec) * 1000000) + 
            (t1.tv_usec - t2.tv_usec))/1000;
}


int main (int argc, char *argv[])
{
    zmq::context_t context (1);

    //  Socket to talk to server
//    std::cout << "Collecting updates from weather server…\n" << std::endl;
    zmq::socket_t subscriber (context, ZMQ_SUB);
    subscriber.connect("tcp://192.168.0.2:123458");
    //  Subscribe to zipcode, default is NYC, 10001
    const char* filter = "";
    subscriber.setsockopt(ZMQ_SUBSCRIBE, filter, strlen(filter));

    //  Process 100 updates
    int update_nbr;
    long total_temp = 0;
	int i = 0;
    while (1)
    {
        zmq::message_t update;
        subscriber.recv(&update);
		timeval now;
		gettimeofday(&now, NULL);
//        std::cerr << "received\n";
//        std::istringstream iss(static_cast<char*>(update.data()));
//        iss >> zipcode >> relhumidity;
		Chunk* chunk = new Chunk();
        memcpy(chunk, update.data(), sizeof(Chunk));

		timeval ts;
		ts.tv_sec = chunk->tv_sec;
		ts.tv_usec = chunk->tv_usec;
		if (i++ == 100)
		{
			std::cerr << diff_ms(now, ts) << "\n" << std::flush;//timeToStr(ts) << "\t" << chunk->tv_usec << "\n";
			i = 0;
		}

//        std::cout << "update\n";
//        for (size_t n=0; n<size; ++n)
//            std::cout << chunk->payload[n] << std::flush;

//		std::cerr << (chunk->timestamp).tv_sec << ":" << (chunk->timestamp).tv_usec << "\n";
		delete chunk;
//        std::cout << std::flush;
//        std::cerr << "flushed\n";
    }
    return 0;
}

