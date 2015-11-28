/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <iostream>
#include <sys/resource.h>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>

#include "common/daemon.h"
#include "common/log.h"
#include "common/signalHandler.h"
#include "controller.h"
#include "alsaPlayer.h"
#include "common/publishAvahi.h"
#include "webServer.h"

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
	try
	{
		string soundcard;
		string ip;
	//	int bufferMs;
		size_t port;
		size_t latency;
		size_t www;
		int runAsDaemon;
		bool listPcmDevices;
		string name;

		po::options_description desc("Allowed options");
		desc.add_options()
		("help,h", "produce help message")
		("version,v", "show version number")
		("list,l", po::bool_switch(&listPcmDevices)->default_value(false), "list pcm devices")
		("ip,i", po::value<string>(&ip), "server IP")
		("port,p", po::value<size_t>(&port)->default_value(98765), "server port")
		("soundcard,s", po::value<string>(&soundcard)->default_value("default"), "index or name of the soundcard")
		("daemon,d", po::value<int>(&runAsDaemon)->implicit_value(-3), "daemonize, optional process priority [-20..19]")
		("latency", po::value<size_t>(&latency)->default_value(0), "latency of the soundcard")
		("name,n", po::value<string>(&name)->default_value("SnapCast Client"), "name for this snapcast client")
    ("www,w", po::value<size_t>(&www)->default_value(8000), "www port")
		;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		std::clog.rdbuf(new Log("snapclient", LOG_DAEMON));

		if (vm.count("help"))
		{
			cout << desc << "\n";
			return 1;
		}

		if (vm.count("version"))
		{
			cout << "snapclient v" << VERSION << "\n"
				 << "Copyright (C) 2014 BadAix (snapcast@badaix.de).\n"
				 << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
				 << "This is free software: you are free to change and redistribute it.\n"
				 << "There is NO WARRANTY, to the extent permitted by law.\n\n"
				 << "Written by Johannes M. Pohl.\n";
			return 1;
		}

		if (listPcmDevices)
		{
			vector<PcmDevice> pcmDevices = Player::pcm_list();
			for (auto dev: pcmDevices)
			{
				cout << dev.idx << ": " << dev.name << "\n"
					 << dev.description << "\n\n";
			}
			return 1;
		}

	    signal(SIGHUP, signal_handler);
	    signal(SIGTERM, signal_handler);
	    signal(SIGINT, signal_handler);

		if (vm.count("daemon"))
		{
			daemonize("/var/run/snapclient.pid");
			if (runAsDaemon < -20)
				runAsDaemon = -20;
			else if (runAsDaemon > 19)
				runAsDaemon = 19;
			setpriority(PRIO_PROCESS, 0, runAsDaemon);
			logS(kLogNotice) << "daemon started" << std::endl;
		}

		PcmDevice pcmDevice = getPcmDevice(soundcard);
		if (pcmDevice.idx == -1)
		{
			cout << "soundcard \"" << soundcard << "\" not found\n";
			return 1;
		}

    PublishAvahi publishAvahi(name);
    std::vector<AvahiService> services;
    services.push_back(AvahiService("_snapcastclient._tcp", www));
    publishAvahi.publish(services);

		std::unique_ptr<WebServer> webserver(new WebServer());
		webserver->startServer(www);

		std::unique_ptr<Controller> controller(new Controller());
		if (!g_terminated)
		{
      /*
			controller->start(pcmDevice, "", ip, port, latency);
                        cout << "Started" << std::endl;
      */
      webserver->ip_ = ip;
      webserver->port_ = port;
			while(!g_terminated) {
        cout << "check for webserver changes" << std::endl;
				if (webserver->ip_ != controller->ip_ || webserver->port_ != controller->port_ || webserver->name_ != controller->name_) {
          cout << webserver->ip_ << ":" << controller->ip_ << std::endl;
          cout << webserver->port_ << ":" << controller->port_ << std::endl;
          cout << webserver->name_ << ":" << controller->name_ << std::endl;
          cout << "stopping due to webserver change" << std::endl;
					controller->stop();
          cout << "starting due to webserver change" << std::endl;
					controller->start(pcmDevice, webserver->name_, webserver->ip_, webserver->port_, latency);
          // controller does a scan so we need to update the webserver with scanned results
          webserver->ip_ = controller->ip_;
          webserver->port_ = controller->port_;
          webserver->name_ = controller->name_;
        }             
        cout << "Sleeping" << std::endl;
        usleep(100*1000);
      }
			cout << "Stopping\n";
			controller->stop();
		}
    webserver->stopServer();
	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception: " << e.what() << std::endl;
	}

	logS(kLogNotice) << "daemon terminated." << endl;
	daemonShutdown();
	return 0;
}


