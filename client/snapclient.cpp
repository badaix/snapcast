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
#ifndef WINDOWS
#include <signal.h>
#include <sys/resource.h>
#endif

#include "common/popl.hpp"
#include "controller.hpp"

#ifdef HAS_ALSA
#include "player/alsa_player.hpp"
#endif
#ifdef HAS_WASAPI
#include "player/wasapi_player.h"
#endif
#ifdef HAS_DAEMON
#include "common/daemon.hpp"
#endif
#include "client_settings.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils.hpp"
#include "metadata.hpp"


using namespace std;
using namespace popl;

using namespace std::chrono_literals;

static constexpr auto LOG_TAG = "Snapclient";

PcmDevice getPcmDevice(const std::string& soundcard)
{
#if defined(HAS_ALSA) || defined(HAS_WASAPI)
    vector<PcmDevice> pcmDevices =
#ifdef HAS_ALSA
        AlsaPlayer::pcm_list();
#else
        WASAPIPlayer::pcm_list();
#endif

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
    std::ignore = soundcard;
#endif

    PcmDevice pcmDevice;
    pcmDevice.name = soundcard;
    return pcmDevice;
}

#ifdef WINDOWS
// hack to avoid case destinction in the signal handler
#define SIGHUP SIGINT
const char* strsignal(int sig)
{
    switch (sig)
    {
        case SIGTERM:
            return "SIGTERM";
        case SIGINT:
            return "SIGINT";
        case SIGBREAK:
            return "SIGBREAK";
        case SIGABRT:
            return "SIGABRT";
        default:
            return "Unhandled";
    }
}
#endif

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
        auto versionSwitch = op.add<Switch>("v", "version", "show version number");
        op.add<Value<string>>("h", "host", "server hostname or ip address", "", &settings.server.host);
        op.add<Value<size_t>>("p", "port", "server port", 1704, &settings.server.port);
        op.add<Value<size_t>>("i", "instance", "instance id when running multiple instances on the same host", 1, &settings.instance);
        op.add<Value<string>>("", "hostID", "unique host id, default is MAC address", "", &settings.host_id);

// PCM device specific
#if defined(HAS_ALSA) || defined(HAS_WASAPI)
        auto listSwitch = op.add<Switch>("l", "list", "list PCM devices");
        /*auto soundcardValue =*/op.add<Value<string>>("s", "soundcard", "index or name of the pcm device", "default", &pcm_device);
#endif
        /*auto latencyValue =*/op.add<Value<int>>("", "latency", "latency of the PCM device", 0, &settings.player.latency);
#ifdef HAS_SOXR
        auto sample_format = op.add<Value<string>>("", "sampleformat", "resample audio stream to <rate>:<bits>:<channels>", "");
#endif

// audio backend
#if defined(HAS_OBOE) && defined(HAS_OPENSL)
        op.add<Value<string>>("", "player", "audio backend (oboe, opensl)", "oboe", &settings.player.player_name);
#else
        op.add<Value<string>, Attribute::hidden>("", "player", "audio backend (<empty>, file)", "", &settings.player.player_name);
#endif

// sharing mode
#if defined(HAS_OBOE) || defined(HAS_WASAPI)
        auto sharing_mode = op.add<Value<string>>("", "sharingmode", "audio mode to use [shared|exclusive]", "shared");
#endif

        // mixer
        bool hw_mixer_supported = false;
#if defined(HAS_ALSA)
        hw_mixer_supported = true;
#endif
        std::shared_ptr<popl::Value<std::string>> mixer_mode;
        if (hw_mixer_supported)
            mixer_mode = op.add<Value<string>>("", "mixer", "software|hardware|script|none|?[:<options>]", "software");
        else
            mixer_mode = op.add<Value<string>>("", "mixer", "software|script|none|?[:<options>]", "software");

        auto metaStderr = op.add<Switch>("e", "mstderr", "send metadata to stderr");

// daemon settings
#ifdef HAS_DAEMON
        int processPriority(-3);
        auto daemonOption = op.add<Implicit<int>>("d", "daemon", "daemonize, optional process priority [-20..19]", processPriority, &processPriority);
        auto userValue = op.add<Value<string>>("", "user", "the user[:group] to run snapclient as when daemonized");
#endif

        // logging
        op.add<Value<string>>("", "logsink", "log sink [null,system,stdout,stderr,file:<filename>]", settings.logging.sink, &settings.logging.sink);
        auto logfilterOption = op.add<Value<string>>(
            "", "logfilter", "log filter <tag>:<level>[,<tag>:<level>]* with tag = * or <log tag> and level = [trace,debug,info,notice,warning,error,fatal]",
            settings.logging.filter);

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

#if defined(HAS_ALSA) || defined(HAS_WASAPI)
        if (listSwitch->is_set())
        {
            vector<PcmDevice> pcmDevices =
#ifdef HAS_ALSA
                AlsaPlayer::pcm_list();
#else
                WASAPIPlayer::pcm_list();
#endif
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

        settings.logging.filter = logfilterOption->value();
        if (logfilterOption->is_set())
        {
            for (size_t n = 1; n < logfilterOption->count(); ++n)
                settings.logging.filter += "," + logfilterOption->value(n);
        }

        if (settings.logging.sink.empty())
        {
            settings.logging.sink = "stdout";
#ifdef HAS_DAEMON
            if (daemonOption->is_set())
                settings.logging.sink = "system";
#endif
        }
        AixLog::Filter logfilter;
        auto filters = utils::string::split(settings.logging.filter, ',');
        for (const auto& filter : filters)
            logfilter.add_filter(filter);

        string logformat = "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)";
        if (settings.logging.sink.find("file:") != string::npos)
        {
            string logfile = settings.logging.sink.substr(settings.logging.sink.find(":") + 1);
            AixLog::Log::init<AixLog::SinkFile>(logfilter, logfile, logformat);
        }
        else if (settings.logging.sink == "stdout")
            AixLog::Log::init<AixLog::SinkCout>(logfilter, logformat);
        else if (settings.logging.sink == "stderr")
            AixLog::Log::init<AixLog::SinkCerr>(logfilter, logformat);
        else if (settings.logging.sink == "system")
            AixLog::Log::init<AixLog::SinkNative>("snapclient", logfilter);
        else if (settings.logging.sink == "null")
            AixLog::Log::init<AixLog::SinkNull>();
        else
            throw SnapException("Invalid log sink: " + settings.logging.sink);

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
            processPriority = std::min(std::max(-20, processPriority), 19);
            if (processPriority != 0)
                setpriority(PRIO_PROCESS, 0, processPriority);
            LOG(NOTICE, LOG_TAG) << "daemonizing" << std::endl;
            daemon->daemonize();
            LOG(NOTICE, LOG_TAG) << "daemon started" << std::endl;
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

#if defined(HAS_OBOE) || defined(HAS_WASAPI)
        settings.player.sharing_mode = (sharing_mode->value() == "exclusive") ? ClientSettings::SharingMode::exclusive : ClientSettings::SharingMode::shared;
#endif

        string mode = utils::string::split_left(mixer_mode->value(), ':', settings.player.mixer.parameter);
        if (mode == "software")
            settings.player.mixer.mode = ClientSettings::Mixer::Mode::software;
        else if ((mode == "hardware") && hw_mixer_supported)
            settings.player.mixer.mode = ClientSettings::Mixer::Mode::hardware;
        else if (mode == "script")
            settings.player.mixer.mode = ClientSettings::Mixer::Mode::script;
        else if (mode == "none")
            settings.player.mixer.mode = ClientSettings::Mixer::Mode::none;
        else if ((mode == "?") || (mode == "help"))
        {
            cout << "mixer can be one of 'software', " << (hw_mixer_supported ? "'hardware', " : "") << "'script', 'none'\n"
                 << "followed by optional parameters:\n"
                 << " * software[:poly[:<exponent>]|exp[:<base>]]\n"
                 << (hw_mixer_supported ? " * hardware[:<mixer name>]\n" : "") << " * script[:<script filename>]\n";
            exit(EXIT_SUCCESS);
        }
        else
            throw SnapException("Mixer mode not supported: " + mode);

        boost::asio::io_context io_context;
        // Construct a signal set registered for process termination.
        boost::asio::signal_set signals(io_context, SIGHUP, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code& ec, int signal) {
            if (!ec)
                LOG(INFO, LOG_TAG) << "Received signal " << signal << ": " << strsignal(signal) << "\n";
            else
                LOG(INFO, LOG_TAG) << "Failed to wait for signal, error: " << ec.message() << "\n";
            io_context.stop();
        });

        // Setup metadata handling
        auto meta(metaStderr ? std::make_unique<MetaStderrAdapter>() : std::make_unique<MetadataAdapter>());
        auto controller = make_shared<Controller>(io_context, settings, std::move(meta));
        controller->start();

        int num_threads = 0;
        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n)
            threads.emplace_back([&] { io_context.run(); });
        io_context.run();
        for (auto& t : threads)
            t.join();
    }
    catch (const std::exception& e)
    {
        LOG(FATAL, LOG_TAG) << "Exception: " << e.what() << std::endl;
        exitcode = EXIT_FAILURE;
    }

    LOG(NOTICE, LOG_TAG) << "daemon terminated." << endl;
    exit(exitcode);
}
