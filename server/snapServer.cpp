/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#include <chrono>
#include <memory>
#include <sys/resource.h>

#include "popl.hpp"
#include "common/daemon.h"
#include "common/timeDefs.h"
#include "common/signalHandler.h"
#include "common/snapException.h"
#include "common/sampleFormat.h"
#include "message/message.h"
#include "encoder/encoderFactory.h"
#include "streamServer.h"
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
#include "publishZeroConf/publishmDNS.h"
#endif
#include "config.h"
#include "common/log.h"


volatile sig_atomic_t g_terminated = false;
std::condition_variable terminateSignaled;

using namespace std;
using namespace popl;


int main(int argc, char* argv[])
{
#ifdef MACOS
#pragma message "Warning: the macOS support is experimental and might not be maintained"
#endif
	try
	{
		StreamServerSettings settings;
		std::string pcmStream = "pipe:///tmp/snapfifo?name=default";
		int processPriority(0);

		Switch helpSwitch("h", "help", "Produce help message");
		Switch versionSwitch("v", "version", "Show version number");
		Value<size_t> portValue("p", "port", "Server port", settings.port, &settings.port);
		Value<size_t> controlPortValue("", "controlPort", "Remote control port", settings.controlPort, &settings.controlPort);
		Value<string> streamValue("s", "stream", "URI of the PCM input stream.\nFormat: TYPE://host/path?name=NAME\n[&codec=CODEC]\n[&sampleformat=SAMPLEFORMAT]", pcmStream, &pcmStream);

		Value<string> sampleFormatValue("", "sampleformat", "Default sample format", settings.sampleFormat);
		Value<string> codecValue("c", "codec", "Default transport codec\n(flac|ogg|pcm)[:options]\nType codec:? to get codec specific options", settings.codec, &settings.codec);
		Value<size_t> streamBufferValue("", "streamBuffer", "Default stream read buffer [ms]", settings.streamReadMs, &settings.streamReadMs);

		Value<int> bufferValue("b", "buffer", "Buffer [ms]", settings.bufferMs, &settings.bufferMs);
		Implicit<int> daemonOption("d", "daemon", "Daemonize\noptional process priority [-20..19]", 0, &processPriority);

		OptionParser op("Allowed options");
		op.add(helpSwitch)
		 .add(versionSwitch)
		 .add(portValue)
		 .add(controlPortValue)
		 .add(streamValue)
		 .add(sampleFormatValue)
		 .add(codecValue)
		 .add(streamBufferValue)
		 .add(bufferValue)
		 .add(daemonOption);

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
			cout << "snapserver v" << VERSION << "\n"
				<< "Copyright (C) 2014-2016 BadAix (snapcast@badaix.de).\n"
				<< "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
				<< "This is free software: you are free to change and redistribute it.\n"
				<< "There is NO WARRANTY, to the extent permitted by law.\n\n"
				<< "Written by Johannes Pohl.\n";
			exit(EXIT_SUCCESS);
		}

		if (helpSwitch.isSet())
		{
			cout << op << "\n";
			exit(EXIT_SUCCESS);
		}

		if (!streamValue.isSet())
			settings.pcmStreams.push_back(streamValue.getValue());

		for (size_t n=0; n<streamValue.count(); ++n)
		{
			cout << streamValue.getValue(n) << "\n";
			settings.pcmStreams.push_back(streamValue.getValue(n));
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

		if (daemonOption.isSet())
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
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
		PublishZeroConf publishZeroConfg("Snapcast");
		publishZeroConfg.publish({mDNSService("_snapcast._tcp", settings.port), mDNSService("_snapcast-jsonrpc._tcp", settings.controlPort)});
#endif

		if (settings.bufferMs < 400)
			settings.bufferMs = 400;
		settings.sampleFormat = sampleFormatValue.getValue();

		asio::io_service io_service;
		std::unique_ptr<StreamServer> streamServer(new StreamServer(&io_service, settings));
		streamServer->start();

		auto func = [](asio::io_service* ioservice)->void{ioservice->run();};
		std::thread t(func, &io_service);

		while (!g_terminated)
			chronos::sleep(100);

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

