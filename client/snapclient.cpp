/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

// local headers
#include "common/popl.hpp"
#include "controller.hpp"

#ifdef HAS_ALSA
#include "player/alsa_player.hpp"
#endif
#ifdef HAS_PULSE
#include "player/pulse_player.hpp"
#endif
#ifdef HAS_WASAPI
#include "player/wasapi_player.hpp"
#endif
#include "player/file_player.hpp"
#ifdef HAS_DAEMON
#include "common/daemon.hpp"
#endif
#include "client_settings.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils.hpp"
#include "common/version.hpp"

// 3rd party headers
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/signal_set.hpp>

// standard headers
#include <chrono>
#include <iostream>
#ifndef WINDOWS
#include <csignal>
#include <sys/resource.h>
#endif


using namespace std;
using namespace popl;
using namespace player;

using namespace std::chrono_literals;

static constexpr auto LOG_TAG = "Snapclient";


PcmDevice getPcmDevice(const std::string& player, const std::string& parameter, const std::string& soundcard)
{
    LOG(DEBUG, LOG_TAG) << "Trying to get PCM device for player: " << player << ", parameter: "
                        << ", card: " << soundcard << "\n";
#if defined(HAS_ALSA) || defined(HAS_PULSE) || defined(HAS_WASAPI)
    vector<PcmDevice> pcm_devices;
#if defined(HAS_ALSA)
    if (player == player::ALSA)
        pcm_devices = AlsaPlayer::pcm_list();
#endif
#if defined(HAS_PULSE)
    if (player == player::PULSE)
        pcm_devices = PulsePlayer::pcm_list(parameter);
#endif
#if defined(HAS_WASAPI)
    if (player == player::WASAPI)
        pcm_devices = WASAPIPlayer::pcm_list();
#endif
    if (player == player::FILE)
        return FilePlayer::pcm_list(parameter).front();
    try
    {
        int soundcardIdx = cpt::stoi(soundcard);
        for (auto dev : pcm_devices)
            if (dev.idx == soundcardIdx)
                return dev;
    }
    catch (...)
    {
    }

    for (auto dev : pcm_devices)
        if (dev.name.find(soundcard) != string::npos)
            return dev;
#endif
    std::ignore = player;
    std::ignore = parameter;
    PcmDevice pcm_device;
    pcm_device.name = soundcard;
    return pcm_device;
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
        ClientSettings settings;
        string pcm_device(player::DEFAULT_DEVICE);

        OptionParser op("Allowed options");
        auto helpSwitch = op.add<Switch>("", "help", "produce help message");
        auto groffSwitch = op.add<Switch, Attribute::hidden>("", "groff", "produce groff message");
        auto versionSwitch = op.add<Switch>("v", "version", "show version number");
        op.add<Value<string>>("h", "host", "server hostname or ip address", "", &settings.server.host);
        op.add<Value<size_t>>("p", "port", "server port", 1704, &settings.server.port);
        op.add<Value<size_t>>("i", "instance", "instance id when running multiple instances on the same host", 1, &settings.instance);
        op.add<Value<string>>("", "hostID", "unique host id, default is MAC address", "", &settings.host_id);

// PCM device specific
#if defined(HAS_ALSA) || defined(HAS_PULSE) || defined(HAS_WASAPI)
        auto listSwitch = op.add<Switch>("l", "list", "list PCM devices");
        /*auto soundcardValue =*/op.add<Value<string>>("s", "soundcard", "index or name of the pcm device", pcm_device, &pcm_device);
#endif
        /*auto latencyValue =*/op.add<Value<int>>("", "latency", "latency of the PCM device", 0, &settings.player.latency);
#ifdef HAS_SOXR
        auto sample_format = op.add<Value<string>>("", "sampleformat", "resample audio stream to <rate>:<bits>:<channels>", "");
#endif

        auto supported_players = Controller::getSupportedPlayerNames();
        string supported_players_str;
        for (const auto& supported_player : supported_players)
            supported_players_str += (!supported_players_str.empty() ? "|" : "") + supported_player;
        op.add<Value<string>>("", "player", supported_players_str + "[:<options>|?]", supported_players.front(), &settings.player.player_name);

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
            cout << "snapclient v" << version::code << (!version::rev().empty() ? (" (rev " + version::rev(8) + ")") : ("")) << "\n"
                 << "Copyright (C) 2014-2022 BadAix (snapcast@badaix.de).\n"
                 << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
                 << "This is free software: you are free to change and redistribute it.\n"
                 << "There is NO WARRANTY, to the extent permitted by law.\n\n"
                 << "Written by Johannes M. Pohl and contributors <https://github.com/badaix/snapcast/graphs/contributors>.\n\n";
            exit(EXIT_SUCCESS);
        }

        settings.player.player_name = utils::string::split_left(settings.player.player_name, ':', settings.player.parameter);

#if defined(HAS_ALSA) || defined(HAS_PULSE) || defined(HAS_WASAPI)
        if (listSwitch->is_set())
        {
            try
            {
                vector<PcmDevice> pcm_devices;
#if defined(HAS_ALSA)
                if (settings.player.player_name == player::ALSA)
                    pcm_devices = AlsaPlayer::pcm_list();
#endif
#if defined(HAS_PULSE)
                if (settings.player.player_name == player::PULSE)
                    pcm_devices = PulsePlayer::pcm_list(settings.player.parameter);
#endif
#if defined(HAS_WASAPI)
                if (settings.player.player_name == player::WASAPI)
                    pcm_devices = WASAPIPlayer::pcm_list();
#endif
#ifdef WINDOWS
                // Set console code page to UTF-8 so console known how to interpret string data
                SetConsoleOutputCP(CP_UTF8);
                // Enable buffering to prevent VS from chopping up UTF-8 byte sequences
                setvbuf(stdout, nullptr, _IOFBF, 1000);
#endif
                for (const auto& dev : pcm_devices)
                    cout << dev.idx << ": " << dev.name << "\n" << dev.description << "\n\n";

                if (pcm_devices.empty())
                    cout << "No PCM device available for audio backend \"" << settings.player.player_name << "\"\n";
            }
            catch (const std::exception& e)
            {
                cout << "Failed to get device list: " << e.what() << "\n";
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
            string logfile = settings.logging.sink.substr(settings.logging.sink.find(':') + 1);
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

#if !defined(HAS_AVAHI) && !defined(HAS_BONJOUR)
        if (settings.server.host.empty())
            throw SnapException("Snapserver host not configured and mDNS not available, please configure with \"--host\".");
#endif


#ifdef HAS_DAEMON
        std::unique_ptr<Daemon> daemon;
        if (daemonOption->is_set())
        {
            string pidFile = "/var/run/snapclient/pid";
            if (settings.instance != 1)
                pidFile += "." + cpt::to_string(settings.instance);
            string user;
            string group;

            if (userValue->is_set())
            {
                if (userValue->value().empty())
                    throw std::invalid_argument("user must not be empty");

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

        if (settings.player.parameter == "?")
        {
            if (settings.player.player_name == player::FILE)
            {
                cout << "Options are a comma separated list of:\n"
                     << " \"filename=<filename>\" - with <filename> = \"stdout\", \"stderr\", \"null\" or a filename\n"
                     << " \"mode=[w|a]\" - w: write (discarding the content), a: append (keeping the content)\n";
            }
#ifdef HAS_PULSE
            else if (settings.player.player_name == player::PULSE)
            {
                cout << "Options are a comma separated list of:\n"
                     << " \"buffer_time=<buffer size [ms]>\" - default 100, min 10\n"
                     << " \"server=<PulseAudio server>\" - default not-set: use the default server\n"
                     << " \"property=<key>=<value>\" - can be set multiple times, default 'media.role=music'\n";
            }
#endif
#ifdef HAS_ALSA
            else if (settings.player.player_name == player::ALSA)
            {
                cout << "Options are a comma separated list of:\n"
                     << " \"buffer_time=<total buffer size [ms]>\" - default 80, min 10\n"
                     << " \"fragments=<number of buffers>\" - default 4, min 2\n";
            }
#endif
            else
            {
                cout << "No options available for \"" << settings.player.player_name << "\n";
            }
            exit(EXIT_SUCCESS);
        }

        settings.player.pcm_device = getPcmDevice(settings.player.player_name, settings.player.parameter, pcm_device);
#if defined(HAS_ALSA)
        if (settings.player.pcm_device.idx == -1)
        {
            LOG(ERROR, LOG_TAG) << "PCM device \"" << pcm_device << "\" not found\n";
            // exit(EXIT_FAILURE);
        }
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
        signals.async_wait(
            [&](const boost::system::error_code& ec, int signal)
            {
            if (!ec)
                LOG(INFO, LOG_TAG) << "Received signal " << signal << ": " << strsignal(signal) << "\n";
            else
                LOG(INFO, LOG_TAG) << "Failed to wait for signal, error: " << ec.message() << "\n";
            io_context.stop();
        });

        LOG(INFO, LOG_TAG) << "Version " << version::code << (!version::rev().empty() ? (", revision " + version::rev(8)) : ("")) << "\n";

        auto controller = make_shared<Controller>(io_context, settings);
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

    LOG(NOTICE, LOG_TAG) << "Snapclient terminated." << endl;
    exit(exitcode);
}
