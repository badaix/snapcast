/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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
#ifdef HAS_DAEMON
#include "common/daemon.h"
#endif
#include "common/timeDefs.h"
#include "common/utils/string_utils.h"
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
#include "aixlog.hpp"


volatile sig_atomic_t g_terminated = false;
std::condition_variable terminateSignaled;

using namespace std;
using namespace popl;


int main(int argc, char* argv[])
{
#ifdef MACOS
#pragma message "Warning: the macOS support is experimental and might not be maintained"
#endif

	int exitcode = EXIT_SUCCESS;
	try
	{
		StreamServerSettings settings;
		std::string pcmStream = "pipe:///tmp/snapfifo?name=default";

		OptionParser op("Allowed options");
		auto helpSwitch =        op.add<Switch>("h", "help", "Produce help message");
		auto groffSwitch =       op.add<Switch, Attribute::hidden>("", "groff", "produce groff message");
		auto debugOption =       op.add<Implicit<string>, Attribute::hidden>("", "debug", "enable debug logging", "");
		auto versionSwitch =     op.add<Switch>("v", "version", "Show version number");
		/*auto portValue =*/         op.add<Value<size_t>>("p", "port", "Server port", settings.port, &settings.port);
		/*auto controlPortValue =*/  op.add<Value<size_t>>("", "controlPort", "Remote control port", settings.controlPort, &settings.controlPort);
		auto streamValue =       op.add<Value<string>>("s", "stream", "URI of the PCM input stream.\nFormat: TYPE://host/path?name=NAME\n[&codec=CODEC]\n[&sampleformat=SAMPLEFORMAT]", pcmStream, &pcmStream);

		/*auto sampleFormatValue =*/ op.add<Value<string>>("", "sampleformat", "Default sample format", settings.sampleFormat, &settings.sampleFormat);
		/*auto codecValue =*/        op.add<Value<string>>("c", "codec", "Default transport codec\n(flac|ogg|pcm)[:options]\nType codec:? to get codec specific options", settings.codec, &settings.codec);
		/*auto streamBufferValue =*/ op.add<Value<size_t>>("", "streamBuffer", "Default stream read buffer [ms]", settings.streamReadMs, &settings.streamReadMs);
		/*auto bufferValue =*/       op.add<Value<int>>("b", "buffer", "Buffer [ms]", settings.bufferMs, &settings.bufferMs);
		/*auto muteSwitch =*/        op.add<Switch>("", "sendToMuted", "Send audio to muted clients", &settings.sendAudioToMutedClients);
#ifdef HAS_DAEMON
		int processPriority(0);
		auto daemonOption =      op.add<Implicit<int>>("d", "daemon", "Daemonize\noptional process priority [-20..19]", 0, &processPriority);
		auto userValue =         op.add<Value<string>>("", "user", "the user[:group] to run snapserver as when daemonized", "");
#endif

		try
		{
			op.parse(argc, argv);
		}
		catch (const std::invalid_argument& e)
		{
			SLOG(ERROR) << "Exception: " << e.what() << std::endl;
			cout << "\n" << op << "\n";
			exit(EXIT_FAILURE);
		}

		if (versionSwitch->is_set())
		{
			cout << "snapserver v" << VERSION << "\n"
				<< "Copyright (C) 2014-2018 BadAix (snapcast@badaix.de).\n"
				<< "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
				<< "This is free software: you are free to change and redistribute it.\n"
				<< "There is NO WARRANTY, to the extent permitted by law.\n\n"
				<< "Written by Johannes Pohl.\n";
			exit(EXIT_SUCCESS);
		}

		if (helpSwitch->is_set())
		{
			cout << op << "\n";
			exit(EXIT_SUCCESS);
		}

		if (groffSwitch->is_set())
		{
			GroffOptionPrinter option_printer(&op);
			cout << option_printer.print();
			exit(EXIT_SUCCESS);
		}

		if (!streamValue->is_set())
			settings.pcmStreams.push_back(streamValue->value());

		for (size_t n=0; n<streamValue->count(); ++n)
		{
			cout << streamValue->value(n) << "\n";
			settings.pcmStreams.push_back(streamValue->value(n));
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

		AixLog::Log::init<AixLog::SinkNative>("snapserver", AixLog::Severity::trace, AixLog::Type::special);
		if (debugOption->is_set())
		{
			AixLog::Log::instance().add_logsink<AixLog::SinkCout>(AixLog::Severity::trace, AixLog::Type::all, "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)");
			if (!debugOption->value().empty())
				AixLog::Log::instance().add_logsink<AixLog::SinkFile>(AixLog::Severity::trace, AixLog::Type::all, debugOption->value(), "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)");
		}
		else
		{
			AixLog::Log::instance().add_logsink<AixLog::SinkCout>(AixLog::Severity::info, AixLog::Type::all, "%Y-%m-%d %H-%M-%S [#severity]");
		}


		signal(SIGHUP, signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGINT, signal_handler);

#ifdef HAS_DAEMON
		std::unique_ptr<Daemon> daemon;
		if (daemonOption->is_set())
		{
			string user = "";
			string group = "";

			if (userValue->is_set())
			{
				if (userValue->value().empty())
					std::invalid_argument("user must not be empty");

				vector<string> user_group = utils::string::split(userValue->value(), ':');
				user = user_group[0];
				if (user_group.size() > 1)
					group = user_group[1];
			}

			Config::instance().init("/var/lib/snapserver", user, group);
			daemon.reset(new Daemon(user, group, "/var/run/snapserver/pid"));
			daemon->daemonize();
			if (processPriority < -20)
				processPriority = -20;
			else if (processPriority > 19)
				processPriority = 19;
			if (processPriority != 0)
				setpriority(PRIO_PROCESS, 0, processPriority);
			SLOG(NOTICE) << "daemon started" << std::endl;
		}
		else
			Config::instance().init();
#else
		Config::instance().init();
#endif


#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
		PublishZeroConf publishZeroConfg("Snapcast");
		publishZeroConfg.publish({mDNSService("_snapcast._tcp", settings.port), mDNSService("_snapcast-jsonrpc._tcp", settings.controlPort)});
#endif

		if (settings.bufferMs < 400)
			settings.bufferMs = 400;

		asio::io_service io_service;
		std::unique_ptr<StreamServer> streamServer(new StreamServer(&io_service, settings));
		streamServer->start();

		auto func = [](asio::io_service* ioservice)->void{ioservice->run();};
		std::thread t(func, &io_service);

		while (!g_terminated)
			chronos::sleep(100);

		io_service.stop();
		t.join();

		LOG(INFO) << "Stopping streamServer" << endl;
		streamServer->stop();
		LOG(INFO) << "done" << endl;
	}
	catch (const std::exception& e)
	{
		SLOG(ERROR) << "Exception: " << e.what() << std::endl;
		exitcode = EXIT_FAILURE;
	}

	SLOG(NOTICE) << "daemon terminated." << endl;
	exit(exitcode);
}

