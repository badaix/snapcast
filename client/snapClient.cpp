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
#include <getopt.h>

#include "controller.h"
#include "player/alsaPlayer.h"
#include "browseAvahi.h"
#include "common/daemon.h"
#include "common/log.h"
#include "common/signalHandler.h"


using namespace std;

bool g_terminated = false;

PcmDevice getPcmDevice(const std::string& soundcard)
{
	vector<PcmDevice> pcmDevices = AlsaPlayer::pcm_list();
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
		bool runAsDaemon(false);
		int processPriority(-3);

		while (1)
		{
			int option_index = 0;
			static struct option long_options[] =
			{
				{"help",      no_argument,       0, '?'},
				{"version",   no_argument,       0, 'v'},
				{"list",      no_argument,       0, 'l'},
				{"host",      required_argument, 0, 'h'},
				{"port",      required_argument, 0, 'p'},
				{"soundcard", required_argument, 0, 's'},
				{"daemon",    optional_argument, 0, 'd'},
				{"latency",   required_argument, 0,  0 },
				{0,           0,                 0,  0 }
			};

			char c = getopt_long(argc, argv, "?vlh:p:s:d::", long_options, &option_index);
			if (c == -1)
				break;

			switch (c)
			{
				case 0:
					if (strcmp(long_options[option_index].name, "latency") == 0)
						latency = atoi(optarg);
					break;

				case 'v':
					cout << "snapclient v" << VERSION << "\n"
						<< "Copyright (C) 2014, 2015 BadAix (snapcast@badaix.de).\n"
						<< "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
						<< "This is free software: you are free to change and redistribute it.\n"
						<< "There is NO WARRANTY, to the extent permitted by law.\n\n"
						<< "Written by Johannes M. Pohl.\n\n";
					exit(EXIT_SUCCESS);

				case 'l':
					{
						vector<PcmDevice> pcmDevices = AlsaPlayer::pcm_list();
						for (auto dev: pcmDevices)
						{
							cout << dev.idx << ": " << dev.name << "\n"
								<< dev.description << "\n\n";
						}
						exit(EXIT_SUCCESS);
					}

				case 'h':
					host = optarg;
					break;

				case 'p':
					port = atoi(optarg);
					break;

				case 's':
					soundcard = optarg;
					break;

				case 'd':
					runAsDaemon = true;
					if (optarg)
						processPriority = atoi(optarg);
					break;

				default: //?
					cout << "Allowed options:\n\n"
						<< "      --help        \t\t produce help message\n"
						<< "  -v, --version     \t\t show version number\n"
						<< "  -l, --list        \t\t list pcm devices\n"
						<< "  -h, --host arg    \t\t server hostname or ip address\n"
						<< "  -p, --port arg (=" << port << ")\t server port\n"
						<< "  -s, --soundcard arg(=" << soundcard << ")\t index or name of the soundcard\n"
						<< "  -d, --daemon [arg(=" << processPriority << ")]\t daemonize, optional process priority [-20..19]\n"
						<< "      --latency arg(=" << latency << ")\t\t latency of the soundcard [ms]\n"
						<< "\n";
					exit(EXIT_FAILURE);
			}
		}


		std::clog.rdbuf(new Log("snapclient", LOG_DAEMON));

		signal(SIGHUP, signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGINT, signal_handler);

		if (runAsDaemon)
		{
			daemonize("/var/run/snapclient.pid");
			if (processPriority < -20)
				processPriority = -20;
			else if (processPriority > 19)
				processPriority = 19;
			if (processPriority != 0)
				setpriority(PRIO_PROCESS, 0, processPriority);
			logS(kLogNotice) << "daemon started" << std::endl;
		}

		PcmDevice pcmDevice = getPcmDevice(soundcard);
		if (pcmDevice.idx == -1)
		{
			cout << "soundcard \"" << soundcard << "\" not found\n";
			exit(EXIT_FAILURE);
		}

		if (host.empty())
		{
			BrowseAvahi browseAvahi;
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
		}

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
	daemonShutdown();
	exit(EXIT_SUCCESS);
}


