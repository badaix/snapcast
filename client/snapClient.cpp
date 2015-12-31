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

#include "popl.hpp"
#include "controller.h"
//#include "player/alsaPlayer.h"
//#include "browseAvahi.h"
//#include "common/daemon.h"
#include "common/log.h"
#include "common/signalHandler.h"


using namespace std;
using namespace popl;

bool g_terminated = false;

PcmDevice getPcmDevice(const std::string& soundcard)
{
/*	vector<PcmDevice> pcmDevices = AlsaPlayer::pcm_list();
	int soundcardIdx = -1;

	try
	{
		soundcardIdx = std::stoi(soundcard);
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
*/
	PcmDevice pcmDevice;
	return pcmDevice;
}


int main (int argc, char **argv)
{
	try
	{
		string soundcard("default");
		string host("");
		size_t port(1704);
		size_t latency(0);
		int processPriority(-3);

		Switch helpSwitch("", "help", "produce help message");
		Switch versionSwitch("v", "version", "show version number");
		Switch listSwitch("l", "list", "list pcm devices");
		Value<string> hostValue("h", "host", "server hostname or ip address", "", &host);
		Value<size_t> portValue("p", "port", "server port", 1704, &port);
		Value<string> soundcardValue("s", "soundcard", "index or name of the soundcard", "default", &soundcard);
		Implicit<int> daemonOption("d", "daemon", "daemonize, optional process priority [-20..19]", -3, &processPriority);
		Value<size_t> latencyValue("", "latency", "latency of the soundcard", 0, &latency);

		OptionParser op("Allowed options");
		op.add(helpSwitch)
		 .add(versionSwitch)
		 .add(listSwitch)
		 .add(hostValue)
		 .add(portValue)
		 .add(soundcardValue)
		 .add(daemonOption)
		 .add(latencyValue);

		try
		{
			op.parse(argc, argv);
		}
		catch (const std::invalid_argument& e)
		{
			logS(kLogErr) << "Exception: " << e.what() << std::endl;
			cout << "\n" << op << "\n";
			exit(EXIT_FAILURE);
		}

		if (versionSwitch.isSet())
		{
			cout << "snapclient v" << VERSION << "\n"
				<< "Copyright (C) 2014, 2015 BadAix (snapcast@badaix.de).\n"
				<< "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
				<< "This is free software: you are free to change and redistribute it.\n"
				<< "There is NO WARRANTY, to the extent permitted by law.\n\n"
				<< "Written by Johannes M. Pohl.\n\n";
			exit(EXIT_SUCCESS);
		}

		if (listSwitch.isSet())
		{
/*			vector<PcmDevice> pcmDevices = AlsaPlayer::pcm_list();
			for (auto dev: pcmDevices)
			{
				cout << dev.idx << ": " << dev.name << "\n"
					<< dev.description << "\n\n";
			}
*/			exit(EXIT_SUCCESS);
		}

		if (helpSwitch.isSet())
		{
			cout << op << "\n";
			exit(EXIT_SUCCESS);
		}

		cout << "1\n";

		logO << "1\n";

		std::clog.rdbuf(new Log("snapclient", LOG_DAEMON));

		signal(SIGHUP, signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGINT, signal_handler);

		logO << "1\n";

		if (daemonOption.isSet())
		{
/*			daemonize("/var/run/snapclient.pid");
			if (processPriority < -20)
				processPriority = -20;
			else if (processPriority > 19)
				processPriority = 19;
			if (processPriority != 0)
				setpriority(PRIO_PROCESS, 0, processPriority);
			logS(kLogNotice) << "daemon started" << std::endl;
*/		}

		PcmDevice pcmDevice = getPcmDevice(soundcard);
		if (pcmDevice.idx == -1)
		{
			cout << "soundcard \"" << soundcard << "\" not found\n";
//			exit(EXIT_FAILURE);
		}

		if (host.empty())
		{
/*			BrowseAvahi browseAvahi;
			AvahiResult avahiResult;
			while (!g_terminated)
			{
				try
				{
					if (browseAvahi.browse("_snapcast._tcp", AVAHI_PROTO_INET, avahiResult, 5000))
					{
						host = avahiResult.ip_;
						port = avahiResult.port_;
						logO << "Found server " << host << ":" << port << "\n";
						break;
					}
				}
				catch (const std::exception& e)
				{
					logS(kLogErr) << "Exception: " << e.what() << std::endl;
				}
				usleep(500*1000);
			}
*/		}

		logO << "host: " << host << "\n";

		std::unique_ptr<Controller> controller(new Controller());
		if (!g_terminated)
		{
			logO << "Latency: " << latency << "\n";
			controller->start(pcmDevice, host, port, latency);
			while(!g_terminated)
				usleep(100*1000);
			controller->stop();
		}
	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception: " << e.what() << std::endl;
	}

	logS(kLogNotice) << "daemon terminated." << endl;
//	daemonShutdown();
	exit(EXIT_SUCCESS);
}


