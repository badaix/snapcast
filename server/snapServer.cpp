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

#include <getopt.h>
#include <chrono>
#include <memory>
#include <sys/resource.h>

#include "common/daemon.h"
#include "common/timeDefs.h"
#include "common/signalHandler.h"
#include "common/snapException.h"
#include "message/sampleFormat.h"
#include "message/message.h"
#include "encoder/encoderFactory.h"
#include "streamServer.h"
#include "publishAvahi.h"
#include "config.h"
#include "common/log.h"


bool g_terminated = false;
std::condition_variable terminateSignaled;

using namespace std;



int main(int argc, char* argv[])
{
	try
	{
		StreamServerSettings settings;
		bool runAsDaemon(false);
		int processPriority(-3);
		string sampleFormat;

		while (1)
		{
			int option_index = 0;
			static struct option long_options[] =
			{
				{"help",           no_argument,       0, 'h'},
				{"version",        no_argument,       0, 'v'},
				{"port",           required_argument, 0, 'p'},
				{"controlPort",    required_argument, 0,  0 },
				{"sampleformat",   required_argument, 0, 's'},
				{"codec",          required_argument, 0, 'c'},
				{"fifo",           required_argument, 0, 'f'},
				{"daemon",         optional_argument, 0, 'd'},
				{"buffer",         required_argument, 0, 'b'},
				{"pipeReadBuffer", required_argument, 0,  0 },
				{0,                0,                 0,  0 }
			};

			char c = getopt_long(argc, argv, "hvp:s:c:f:d::b:", long_options, &option_index);
			if (c == -1)
				break;

			switch (c)
			{
				case 0:
					if (strcmp(long_options[option_index].name, "controlPort") == 0)
						settings.controlPort = atoi(optarg);
					else if (strcmp(long_options[option_index].name, "pipeReadBuffer") == 0)
						settings.pipeReadMs = atoi(optarg);
					break;

				case 'v':
					cout << "snapserver " << VERSION << "\n"
						<< "Copyright (C) 2014, 2015 BadAix (snapcast@badaix.de).\n"
						<< "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
						<< "This is free software: you are free to change and redistribute it.\n"
						<< "There is NO WARRANTY, to the extent permitted by law.\n\n"
						<< "Written by Johannes Pohl.\n";
					exit(EXIT_SUCCESS);

				case 'p':
					settings.port = atoi(optarg);
					break;

				case 's':
					settings.sampleFormat = string(optarg);
					break;

				case 'c':
					settings.codec = optarg;
					break;

				case 'f':
					settings.fifoName = optarg;
					break;

				case 'd':
					runAsDaemon = true;
					if (optarg)
						processPriority = atoi(optarg);
					break;

				case 'b':
					settings.bufferMs = atoi(optarg);
					break;

				default: //?
					cout << "Allowed options:\n\n"
						<< "  -h, --help        \t\t\t produce help message\n"
						<< "  -v, --version     \t\t\t show version number\n"
						<< "  -p, --port arg (=" << settings.port << ")\t\t server port\n"
						<< "      --controlPort arg (=" << settings.controlPort << ")\t\t Remote control port\n"
						<< "  -s, --sampleformat arg(=" << settings.sampleFormat.getFormat() << ")\t sample format\n"
						<< "  -c, --codec arg(=" << settings.codec << ")\t\t transport codec [flac|ogg|pcm][:options]. Type codec:? to get codec specific options\n"
						<< "  -f, --fifo arg(=" << settings.fifoName << ")\t name of the input fifo file\n"
						<< "  -d, --daemon [arg(=" << processPriority << ")]\t\t daemonize, optional process priority [-20..19]\n"
						<< "  -b, --buffer arg(=" << settings.bufferMs << ")\t\t buffer [ms]\n"
						<< "      --pipeReadBuffer arg(=" << settings.pipeReadMs << ")\t\t pipe read buffer [ms]\n"
						<< "\n";
					exit(EXIT_FAILURE);
			}
		}

		if (settings.codec.find(":?") != string::npos)
		{
			EncoderFactory encoderFactory;
			std::unique_ptr<Encoder> encoder(encoderFactory.createEncoder(settings.codec));
			if (encoder)
			{
				cout << "Options for codec \"" << encoder->name() << "\":\n"
					<< "  " << encoder->getAvailableOptions() << "\n"
					<< "  Default: \"" << encoder->getDefaultOptions() << "\"\n";
			}
			return 1;
		}

		Config::instance();
		std::clog.rdbuf(new Log("snapserver", LOG_DAEMON));

		signal(SIGHUP, signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGINT, signal_handler);

		if (runAsDaemon)
		{
			daemonize("/var/run/snapserver.pid");
			if (processPriority < -20)
				processPriority = -20;
			else if (processPriority > 19)
				processPriority = 19;
			if (processPriority != 0)
				setpriority(PRIO_PROCESS, 0, processPriority);
			logS(kLogNotice) << "daemon started" << std::endl;
		}

		PublishAvahi publishAvahi("SnapCast");
		std::vector<AvahiService> services;
		services.push_back(AvahiService("_snapcast._tcp", settings.port));
		services.push_back(AvahiService("_snapcast-jsonrpc._tcp", settings.controlPort));
		publishAvahi.publish(services);

		if (settings.bufferMs < 400)
			settings.bufferMs = 400;

		asio::io_service io_service;
		std::unique_ptr<StreamServer> streamServer(new StreamServer(&io_service, settings));
		streamServer->start();

		auto func = [](asio::io_service* ioservice)->void{ioservice->run();};
		std::thread t(func, &io_service);

		while (!g_terminated)
			usleep(100*1000);

		io_service.stop();
		t.join();


		logO << "Stopping streamServer" << endl;
		streamServer->stop();
		logO << "done" << endl;
	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception: " << e.what() << std::endl;
	}

	logS(kLogNotice) << "daemon terminated." << endl;
	daemonShutdown();
	return 0;
}





