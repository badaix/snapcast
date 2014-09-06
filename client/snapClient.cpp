//
//  Weather update client in C++
//  Connects SUB socket to tcp://localhost:5556
//  Collects weather updates and finds avg temp in zipcode
//
//  Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>
//
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "common/sampleFormat.h"
#include "common/utils.h"
#include "common/log.h"
#include "stream.h"
#include "player.h"
#include "serverConnection.h"



using namespace std;
namespace po = boost::program_options;



int main (int argc, char *argv[])
{
	int deviceIdx;
	Stream* stream;
	string ip;	
	int bufferMs;
	size_t port;	
	bool runAsDaemon;
	string sampleFormat;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message")
        ("port,p", po::value<size_t>(&port)->default_value(98765), "port where the server listens on")
        ("ip,i", po::value<string>(&ip)->default_value("192.168.0.2"), "server IP")
        ("soundcard,s", po::value<int>(&deviceIdx)->default_value(-1), "index of the soundcard")
        ("sampleformat,f", po::value<string>(&sampleFormat)->default_value("48000:16:2"), "sample format")
        ("buffer,b", po::value<int>(&bufferMs)->default_value(300), "buffer size [ms]")
        ("daemon,d", po::bool_switch(&runAsDaemon)->default_value(false), "daemonize")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        cout << desc << "\n";
        return 1;
    }

	std::clog.rdbuf(new Log("snapclient", LOG_DAEMON));
	if (runAsDaemon)
	{
		daemonize();
		std::clog << kLogNotice << "daemon started" << std::endl;
	}

	stream = new Stream(SampleFormat(sampleFormat));
	stream->setBufferLen(bufferMs);

	Player player(stream);
	player.start();
	ServerConnection serverConnection(stream);
	serverConnection.start(&serverConnection, ip, port);

	while(true)
		usleep(1000);
	
    return 0;
}


