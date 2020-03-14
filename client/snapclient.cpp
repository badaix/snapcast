/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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
#include <iostream>
#include <sys/resource.h>

#include "browseZeroConf/browse_mdns.hpp"
#include "common/popl.hpp"
#include "controller.hpp"

#ifdef HAS_ALSA
#include "player/alsa_player.hpp"
#endif
#ifdef HAS_DAEMON
#include "common/daemon.hpp"
#endif
#include "client_settings.hpp"
#include "common/aixlog.hpp"
#include "common/signal_handler.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils.hpp"
#include "metadata.hpp"


using namespace std;
using namespace popl;

using namespace std::chrono_literals;

PcmDevice getPcmDevice(const std::string& soundcard)
{
#ifdef HAS_ALSA
    vector<PcmDevice> pcmDevices = AlsaPlayer::pcm_list();

    try
    {
        int soundcardIdx = cpt::stoi(soundcard);
        for (auto dev : pcmDevices)
            if (dev.idx == soundcardIdx)
                return dev;
    }
    catch (...)
    {
    }

    for (auto dev : pcmDevices)
        if (dev.name.find(soundcard) != string::npos)
            return dev;
#else
    std::ignore = soundcard;
#endif

    PcmDevice pcmDevice;
    pcmDevice.name = soundcard;
    return pcmDevice;
}


int main(int argc, char** argv)
{
#ifdef MACOS
#pragma message "Warning: the macOS support is experimental and might not be maintained"
#endif
    int exitcode = EXIT_SUCCESS;
    try
    {
        string meta_script("");
        ClientSettings settings;
        string pcm_device("default");

        OptionParser op("Allowed options");
        auto helpSwitch = op.add<Switch>("", "help", "produce help message");
        auto groffSwitch = op.add<Switch, Attribute::hidden>("", "groff", "produce groff message");
        auto debugOption = op.add<Implicit<string>, Attribute::hidden>("", "debug", "enable debug logging", ""); // TODO: &settings.logging.debug);
        auto versionSwitch = op.add<Switch>("v", "version", "show version number");
#if defined(HAS_ALSA)
        auto listSwitch = op.add<Switch>("l", "list", "list PCM devices");
        /*auto soundcardValue =*/op.add<Value<string>>("s", "soundcard", "index or name of the pcm device", "default", &pcm_device);
#endif
        auto metaStderr = op.add<Switch>("e", "mstderr", "send metadata to stderr");
        /*auto hostValue =*/op.add<Value<string>>("h", "host", "server hostname or ip address", "", &settings.server.host);
        /*auto portValue =*/op.add<Value<size_t>>("p", "port", "server port", 1704, &settings.server.port);
#ifdef HAS_DAEMON
        int processPriority(-3);
        auto daemonOption = op.add<Implicit<int>>("d", "daemon", "daemonize, optional process priority [-20..19]", processPriority, &processPriority);
        auto userValue = op.add<Value<string>>("", "user", "the user[:group] to run snapclient as when daemonized");
#endif
        /*auto latencyValue =*/op.add<Value<int>>("", "latency", "latency of the PCM device", 0, &settings.player.latency);
        /*auto instanceValue =*/op.add<Value<size_t>>("i", "instance", "instance id", 1, &settings.instance);
        /*auto hostIdValue =*/op.add<Value<string>>("", "hostID", "unique host id", "", &settings.host_id);
        op.add<Value<string>>("", "player", "audio backend", "", &settings.player.player_name);
#ifdef HAS_SOXR
        auto sample_format = op.add<Value<string>>("", "sampleformat", "resample audio stream to <rate>:<bits>:<channels>", "");
#endif

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
                 << "Copyright (C) 2014-2020 BadAix (snapcast@badaix.de).\n"
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
            for (auto dev : pcmDevices)
            {
                cout << dev.idx << ": " << dev.name << "\n" << dev.description << "\n\n";
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

        // XXX: Only one metadata option must be set

        AixLog::Log::init<AixLog::SinkNative>("snapclient", AixLog::Severity::trace, AixLog::Type::special);
        if (debugOption->is_set())
        {
            AixLog::Log::instance().add_logsink<AixLog::SinkCout>(AixLog::Severity::trace, AixLog::Type::all, "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)");
            if (!debugOption->value().empty())
                AixLog::Log::instance().add_logsink<AixLog::SinkFile>(AixLog::Severity::trace, AixLog::Type::all, debugOption->value(),
                                                                      "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)");
        }
        else
        {
            AixLog::Log::instance().add_logsink<AixLog::SinkCout>(AixLog::Severity::info, AixLog::Type::all, "%Y-%m-%d %H-%M-%S [#severity] (#tag_func)");
        }

#ifdef HAS_DAEMON
        std::unique_ptr<Daemon> daemon;
        if (daemonOption->is_set())
        {
            string pidFile = "/var/run/snapclient/pid";
            if (settings.instance != 1)
                pidFile += "." + cpt::to_string(settings.instance);
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
            daemon = std::make_unique<Daemon>(user, group, pidFile);
            SLOG(NOTICE) << "daemonizing" << std::endl;
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

        settings.player.pcm_device = getPcmDevice(pcm_device);
#if defined(HAS_ALSA)
        if (settings.player.pcm_device.idx == -1)
        {
            cout << "PCM device \"" << pcm_device << "\" not found\n";
            //			exit(EXIT_FAILURE);
        }
#endif

#ifdef HAS_SOXR
        if (sample_format->is_set())
        {
            settings.player.sample_format = SampleFormat(sample_format->value());
            if (settings.player.sample_format.channels() != 0)
                throw SnapException("sampleformat channels must be * (= same as the source)");
            auto bits = settings.player.sample_format.bits();
            if ((bits != 0) && (bits != 16) && (bits != 24) && (bits != 32))
                throw SnapException("sampleformat bits must be 16, 24, 32, * (= same as the source)");
        }
#endif

        bool active = true;
        std::shared_ptr<Controller> controller;
        auto signal_handler = install_signal_handler({SIGHUP, SIGTERM, SIGINT},
                                                     [&active, &controller](int signal, const std::string& strsignal) {
                                                         SLOG(INFO) << "Received signal " << signal << ": " << strsignal << "\n";
                                                         active = false;
                                                         if (controller)
                                                         {
                                                             LOG(INFO) << "Stopping controller\n";
                                                             controller->stop();
                                                         }
                                                     });
        if (settings.server.host.empty())
        {
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
            BrowseZeroConf browser;
            mDNSResult avahiResult;
            while (active)
            {
                signal_handler.wait_for(500ms);
                if (!active)
                    break;
                try
                {
                    if (browser.browse("_snapcast._tcp", avahiResult, 5000))
                    {
                        settings.server.host = avahiResult.ip;
                        settings.server.port = avahiResult.port;
                        if (avahiResult.ip_version == IPVersion::IPv6)
                            settings.server.host += "%" + cpt::to_string(avahiResult.iface_idx);
                        LOG(INFO) << "Found server " << settings.server.host << ":" << settings.server.port << "\n";
                        break;
                    }
                }
                catch (const std::exception& e)
                {
                    SLOG(ERROR) << "Exception: " << e.what() << std::endl;
                }
            }
#endif
        }

        if (active)
        {
            // Setup metadata handling
            auto meta(metaStderr ? std::make_unique<MetaStderrAdapter>() : std::make_unique<MetadataAdapter>());

            controller = make_shared<Controller>(settings, std::move(meta));
            LOG(INFO) << "Latency: " << settings.player.latency << "\n";
            controller->run();
            // signal_handler.wait();
            // controller->stop();
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
