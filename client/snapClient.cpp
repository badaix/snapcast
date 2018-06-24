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

#include <iostream>
#include <sys/resource.h>

#include "popl.hpp"
#include "controller.h"
#include "browseZeroConf/browsemDNS.h"

#ifdef HAS_ALSA
#include "player/alsaPlayer.h"
#endif
#ifdef HAS_DAEMON
#include "common/daemon.h"
#endif
#include "aixlog.hpp"
#include "common/signalHandler.h"
#include "common/strCompat.h"
#include "common/utils.h"
#include "metadata.h"


using namespace std;
using namespace popl;

volatile sig_atomic_t g_terminated = false;

PcmDevice getPcmDevice(const std::string& soundcard)
{
#ifdef HAS_ALSA
	vector<PcmDevice> pcmDevices = AlsaPlayer::pcm_list();

	try
	{
		int soundcardIdx = cpt::stoi(soundcard);
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
#endif
	PcmDevice pcmDevice;
	return pcmDevice;
}


int main (int argc, char **argv)
{
#ifdef MACOS
#pragma message "Warning: the macOS support is experimental and might not be maintained"
#endif
	int exitcode = EXIT_SUCCESS;
	try
	{
		string meta_script("");
		string soundcard("default");
		string host("");
		size_t port(1704);
		int latency(0);
		size_t instance(1);

		OptionParser op("Allowed options");
		auto helpSwitch =     op.add<Switch>("", "help", "produce help message");
		auto groffSwitch =    op.add<Switch, Attribute::hidden>("", "groff", "produce groff message");
		auto debugOption =    op.add<Implicit<string>, Attribute::hidden>("", "debug", "enable debug logging", "");
		auto versionSwitch =  op.add<Switch>("v", "version", "show version number");
#if defined(HAS_ALSA)
		auto listSwitch =     op.add<Switch>("l", "list", "list pcm devices");
		/*auto soundcardValue =*/ op.add<Value<string>>("s", "soundcard", "index or name of the soundcard", "default", &soundcard);
#endif
		auto metaStderr =     op.add<Switch>("e", "mstderr", "send metadata to stderr");
		//auto metaHook =       op.add<Value<string>>("m", "mhook", "script to call on meta tags", "", &meta_script);
		/*auto hostValue =*/  op.add<Value<string>>("h", "host", "server hostname or ip address", "", &host);
		/*auto portValue =*/  op.add<Value<size_t>>("p", "port", "server port", 1704, &port);
#ifdef HAS_DAEMON
		int processPriority(-3);
		auto daemonOption =   op.add<Implicit<int>>("d", "daemon", "daemonize, optional process priority [-20..19]", -3, &processPriority);
		auto userValue =      op.add<Value<string>>("", "user", "the user[:group] to run snapclient as when daemonized");
#endif
		/*auto latencyValue =*/   op.add<Value<int>>("", "latency", "latency of the soundcard", 0, &latency);
		/*auto instanceValue =*/  op.add<Value<size_t>>("i", "instance", "instance id", 1, &instance);
		auto hostIdValue =    op.add<Value<string>>("", "hostID", "unique host id", "");

		try
		{
			op.parse(argc, argv);
		}
		catch (const std::invalid_argument& e)
		{
			cerr << "Exception: " << e.what() << std::endl;
			cout << "\n" << op << "\n";
			exit(EXIT_FAILURE);
		}

		if (versionSwitch->is_set())
		{
			cout << "snapclient v" << VERSION << "\n"
				<< "Copyright (C) 2014-2018 BadAix (snapcast@badaix.de).\n"
				<< "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
				<< "This is free software: you are free to change and redistribute it.\n"
				<< "There is NO WARRANTY, to the extent permitted by law.\n\n"
				<< "Written by Johannes M. Pohl.\n\n";
			exit(EXIT_SUCCESS);
		}

#ifdef HAS_ALSA
		if (listSwitch->is_set())
		{
			vector<PcmDevice> pcmDevices = AlsaPlayer::pcm_list();
			for (auto dev: pcmDevices)
			{
				cout << dev.idx << ": " << dev.name << "\n"
					<< dev.description << "\n\n";
			}
			exit(EXIT_SUCCESS);
		}
#endif

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

		if (instance <= 0)
			std::invalid_argument("instance id must be >= 1");


		// XXX: Only one metadata option must be set

		AixLog::Log::init<AixLog::SinkNative>("snapclient", AixLog::Severity::trace, AixLog::Type::special);
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
			string pidFile = "/var/run/snapclient/pid";
			if (instance != 1)
				pidFile += "." + cpt::to_string(instance);
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
			daemon.reset(new Daemon(user, group, pidFile));
			daemon->daemonize();
			if (processPriority < -20)
				processPriority = -20;
			else if (processPriority > 19)
				processPriority = 19;
			if (processPriority != 0)
				setpriority(PRIO_PROCESS, 0, processPriority);
			SLOG(NOTICE) << "daemon started" << std::endl;
		}
#endif

		PcmDevice pcmDevice = getPcmDevice(soundcard);
#if defined(HAS_ALSA)
		if (pcmDevice.idx == -1)
		{
			cout << "soundcard \"" << soundcard << "\" not found\n";
//			exit(EXIT_FAILURE);
		}
#endif

		if (host.empty())
		{
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
			BrowseZeroConf browser;
			mDNSResult avahiResult;
			while (!g_terminated)
			{
				try
				{
					if (browser.browse("_snapcast._tcp", avahiResult, 5000))
					{
						host = avahiResult.ip;
						port = avahiResult.port;
						if (avahiResult.ip_version == IPVersion::IPv6)
							host += "%" + cpt::to_string(avahiResult.iface_idx);
						LOG(INFO) << "Found server " << host << ":" << port << "\n";
						break;
					}
				}
				catch (const std::exception& e)
				{
					SLOG(ERROR) << "Exception: " << e.what() << std::endl;
				}
				chronos::sleep(500);
			}
#endif
		}

		// Setup metadata handling
		std::shared_ptr<MetadataAdapter> meta;
		meta.reset(new MetadataAdapter);
		if(metaStderr)
			meta.reset(new MetaStderrAdapter);

		std::unique_ptr<Controller> controller(new Controller(hostIdValue->value(), instance, meta));
		if (!g_terminated)
		{
			LOG(INFO) << "Latency: " << latency << "\n";
			controller->start(pcmDevice, host, port, latency);
			while(!g_terminated)
				chronos::sleep(100);
			controller->stop();
		}
	}
	catch (const std::exception& e)
	{
		SLOG(ERROR) << "Exception: " << e.what() << std::endl;
		exitcode = EXIT_FAILURE;
	}

	SLOG(NOTICE) << "daemon terminated." << endl;
	exit(exitcode);
}


