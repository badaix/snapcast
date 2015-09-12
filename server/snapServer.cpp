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

#include <boost/program_options.hpp>
#include <chrono>
#include <memory>

#include <sys/resource.h>
#include "common/timeDefs.h"
#include "common/signalHandler.h"
#include "common/daemon.h"
#include "common/log.h"
#include "common/snapException.h"
#include "message/sampleFormat.h"
#include "message/message.h"
#include "encoder/encoderFactory.h"
#include "streamServer.h"
#include "publishAvahi.h"
#include "config.h"


bool g_terminated = false;

namespace po = boost::program_options;

using namespace std;


std::condition_variable terminateSignaled;

int main(int argc, char* argv[])
{
	try
	{
		StreamServerSettings settings;
		int runAsDaemon;
		string sampleFormat;

		po::options_description desc("Allowed options");
		desc.add_options()
		("help,h", "produce help message")
		("version,v", "show version number")
		("port,p", po::value<size_t>(&settings.port)->default_value(settings.port), "server port")
		("controlPort", po::value<size_t>(&settings.controlPort)->default_value(settings.controlPort), "Remote control port")
		("sampleformat,s", po::value<string>(&sampleFormat)->default_value(settings.sampleFormat.getFormat()), "sample format")
		("codec,c", po::value<string>(&settings.codec)->default_value(settings.codec), "transport codec [flac|ogg|pcm][:options]. Type codec:? to get codec specific options")
		("fifo,f", po::value<string>(&settings.fifoName)->default_value(settings.fifoName), "name of the input fifo file")
		("daemon,d", po::value<int>(&runAsDaemon)->implicit_value(-3), "daemonize, optional process priority [-20..19]")
		("buffer,b", po::value<int32_t>(&settings.bufferMs)->default_value(settings.bufferMs), "buffer [ms]")
		("pipeReadBuffer", po::value<size_t>(&settings.pipeReadMs)->default_value(settings.pipeReadMs), "pipe read buffer [ms]")
		;

		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if (vm.count("help"))
		{
			cout << desc << "\n";
			return 1;
		}

		if (vm.count("version"))
		{
			cout << "snapserver " << VERSION << "\n"
				 << "Copyright (C) 2014, 2015 BadAix (snapcast@badaix.de).\n"
				 << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
				 << "This is free software: you are free to change and redistribute it.\n"
				 << "There is NO WARRANTY, to the extent permitted by law.\n\n"
				 << "Written by Johannes Pohl.\n";
			return 1;
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

		if (vm.count("daemon"))
		{
			daemonize("/var/run/snapserver.pid");
			if (runAsDaemon < -20)
				runAsDaemon = -20;
			else if (runAsDaemon > 19)
				runAsDaemon = 19;
			setpriority(PRIO_PROCESS, 0, runAsDaemon);
			logS(kLogNotice) << "daemon started." << endl;
		}

		PublishAvahi publishAvahi("SnapCast");
		std::vector<AvahiService> services;
		services.push_back(AvahiService("_snapcast._tcp", settings.port));
		services.push_back(AvahiService("_snapcast-jsonrpc._tcp", settings.controlPort));
		publishAvahi.publish(services);

		if (settings.bufferMs < 400)
			settings.bufferMs = 400;
		settings.sampleFormat = sampleFormat;

		boost::asio::io_service io_service;
		std::unique_ptr<StreamServer> streamServer(new StreamServer(&io_service, settings));
		streamServer->start();

		auto func = [](boost::asio::io_service* ioservice)->void{ioservice->run();};
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





