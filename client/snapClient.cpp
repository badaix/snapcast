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

#include "common/utils.h"
#include "common/log.h"
#include "common/signalHandler.h"
#include "controller.h"
#include "alsaPlayer.h"


using namespace std;
namespace po = boost::program_options;

bool g_terminated = false;

PcmDevice getPcmDevice(const std::string& soundcard)
{
	vector<PcmDevice> pcmDevices = Player::pcm_list();
	int soundcardIdx = -1;

	try
	{
		soundcardIdx = boost::lexical_cast<int>(soundcard);
		for (auto dev: pcmDevices)
			if (dev.idx == soundcardIdx)
				return dev;
	}
	catch(...)
	{
	}

	for (auto dev: pcmDevices)
		if (dev.name.find(soundcard) != string::npos)
			return dev;

	PcmDevice pcmDevice;
	return pcmDevice;
}


int main (int argc, char *argv[])
{
	string soundcard;
	string ip;
//	int bufferMs;
	size_t port;
	size_t latency;
	bool runAsDaemon;
	bool listPcmDevices;

	po::options_description desc("Allowed options");
	desc.add_options()
	("help,h", "produce help message")
	("list,l", po::bool_switch(&listPcmDevices)->default_value(false), "list pcm devices")
	("ip,i", po::value<string>(&ip)->default_value("192.168.0.2"), "server IP")
	("port,p", po::value<size_t>(&port)->default_value(98765), "server port")
	("soundcard,s", po::value<string>(&soundcard)->default_value("default"), "index or name of the soundcard")
	("daemon,d", po::bool_switch(&runAsDaemon)->default_value(false), "daemonize")
	("latency", po::value<size_t>(&latency)->default_value(0), "latency")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help"))
	{
		cout << desc << "\n";
		return 1;
	}

	if (listPcmDevices)
	{
		vector<PcmDevice> pcmDevices = Player::pcm_list();
		for (auto dev: pcmDevices)
		{
			cout << dev.idx << ": " << dev.name << "\n";
			cout << dev.description << "\n\n";
		}
		return 1;
	}

    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

	std::clog.rdbuf(new Log("snapclient", LOG_DAEMON));
	if (runAsDaemon)
	{
		daemonize();
		std::clog << kLogNotice << "daemon started" << std::endl;
	}

	PcmDevice pcmDevice = getPcmDevice(soundcard);
	if (pcmDevice.idx == -1)
	{
		cout << "soundcard \"" << soundcard << "\" not found\n";
		return 1;
	}


	Controller controller;
	controller.start(pcmDevice, ip, port, latency);

	while(!g_terminated)
		usleep(100*1000);

	controller.stop();

	return 0;
}


