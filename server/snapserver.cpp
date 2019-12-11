/***
    This file is part of snapcast
    Copyright (C) 2014-2019  Johannes Pohl

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

#include "common/popl.hpp"
#ifdef HAS_DAEMON
#include "common/daemon.hpp"
#endif
#include "common/sample_format.hpp"
#include "common/signal_handler.hpp"
#include "common/snap_exception.hpp"
#include "common/time_defs.hpp"
#include "common/utils/string_utils.hpp"
#include "encoder/encoder_factory.hpp"
#include "message/message.hpp"
#include "server_settings.hpp"
#include "stream_server.hpp"
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
#include "publishZeroConf/publish_mdns.hpp"
#endif
#include "common/aixlog.hpp"
#include "config.hpp"


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
        ServerSettings settings;
        std::string pcmStream = "pipe:///tmp/snapfifo?name=default";
        std::string config_file = "/etc/snapserver.conf";

        OptionParser op("Allowed options");
        auto helpSwitch = op.add<Switch>("h", "help", "Produce help message");
        auto groffSwitch = op.add<Switch, Attribute::hidden>("", "groff", "produce groff message");
        auto versionSwitch = op.add<Switch>("v", "version", "Show version number");
#ifdef HAS_DAEMON
        int processPriority(0);
        auto daemonOption = op.add<Implicit<int>>("d", "daemon", "Daemonize\noptional process priority [-20..19]", 0, &processPriority);
        auto userValue = op.add<Value<string>>("", "user", "the user[:group] to run snapserver as when daemonized", "");
#endif

        op.add<Value<string>>("c", "config", "path to the configuration file", config_file, &config_file);

        // debug settings
        OptionParser conf("");
        conf.add<Value<bool>>("", "logging.debug", "enable debug logging", settings.logging.debug, &settings.logging.debug);
        conf.add<Value<string>>("", "logging.debug_logfile", "log file name for the debug logs (debug must be enabled)", settings.logging.debug_logfile,
                                &settings.logging.debug_logfile);

        // stream settings
        conf.add<Value<size_t>>("p", "stream.port", "Server port", settings.stream.port, &settings.stream.port);
        auto streamValue = conf.add<Value<string>>(
            "s", "stream.stream", "URI of the PCM input stream.\nFormat: TYPE://host/path?name=NAME\n[&codec=CODEC]\n[&sampleformat=SAMPLEFORMAT]", pcmStream,
            &pcmStream);
        int num_threads = -1;
        conf.add<Value<int>>("", "server.threads", "number of server threads", num_threads, &num_threads);

        conf.add<Value<string>>("", "stream.sampleformat", "Default sample format", settings.stream.sampleFormat, &settings.stream.sampleFormat);
        conf.add<Value<string>>("c", "stream.codec", "Default transport codec\n(flac|ogg|opus|pcm)[:options]\nType codec:? to get codec specific options",
                                settings.stream.codec, &settings.stream.codec);
        // deprecated: stream_buffer, use chunk_ms instead
        conf.add<Value<size_t>>("", "stream.stream_buffer", "Default stream read chunk size [ms]", settings.stream.streamChunkMs,
                                &settings.stream.streamChunkMs);
        conf.add<Value<size_t>>("", "stream.chunk_ms", "Default stream read chunk size [ms]", settings.stream.streamChunkMs, &settings.stream.streamChunkMs);
        conf.add<Value<int>>("b", "stream.buffer", "Buffer [ms]", settings.stream.bufferMs, &settings.stream.bufferMs);
        conf.add<Value<bool>>("", "stream.send_to_muted", "Send audio to muted clients", settings.stream.sendAudioToMutedClients,
                              &settings.stream.sendAudioToMutedClients);
        auto stream_bind_to_address = conf.add<Value<string>>("", "stream.bind_to_address", "address for the server to listen on",
                                                              settings.stream.bind_to_address.front(), &settings.stream.bind_to_address[0]);

        // HTTP RPC settings
        conf.add<Value<bool>>("", "http.enabled", "enable HTTP Json RPC (HTTP POST and websockets)", settings.http.enabled, &settings.http.enabled);
        conf.add<Value<size_t>>("", "http.port", "which port the server should listen to", settings.http.port, &settings.http.port);
        auto http_bind_to_address = conf.add<Value<string>>("", "http.bind_to_address", "address for the server to listen on",
                                                            settings.http.bind_to_address.front(), &settings.http.bind_to_address[0]);
        conf.add<Value<string>>("", "http.doc_root", "serve a website from the doc_root location", settings.http.doc_root, &settings.http.doc_root);

        // TCP RPC settings
        conf.add<Value<bool>>("", "tcp.enabled", "enable TCP Json RPC)", settings.tcp.enabled, &settings.tcp.enabled);
        conf.add<Value<size_t>>("", "tcp.port", "which port the server should listen to", settings.tcp.port, &settings.tcp.port);
        auto tcp_bind_to_address = conf.add<Value<string>>("", "tcp.bind_to_address", "address for the server to listen on",
                                                           settings.tcp.bind_to_address.front(), &settings.tcp.bind_to_address[0]);

        // TODO: Should be possible to override settings on command line

        try
        {
            op.parse(argc, argv);
            conf.parse(config_file);
            if (tcp_bind_to_address->is_set())
            {
                settings.tcp.bind_to_address.clear();
                for (size_t n = 0; n < tcp_bind_to_address->count(); ++n)
                    settings.tcp.bind_to_address.push_back(tcp_bind_to_address->value(n));
            }
            if (http_bind_to_address->is_set())
            {
                settings.http.bind_to_address.clear();
                for (size_t n = 0; n < http_bind_to_address->count(); ++n)
                    settings.http.bind_to_address.push_back(http_bind_to_address->value(n));
            }
            if (stream_bind_to_address->is_set())
            {
                settings.stream.bind_to_address.clear();
                for (size_t n = 0; n < stream_bind_to_address->count(); ++n)
                    settings.stream.bind_to_address.push_back(stream_bind_to_address->value(n));
            }
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
                 << "Copyright (C) 2014-2019 BadAix (snapcast@badaix.de).\n"
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

        if (settings.stream.codec.find(":?") != string::npos)
        {
            encoder::EncoderFactory encoderFactory;
            std::unique_ptr<encoder::Encoder> encoder(encoderFactory.createEncoder(settings.stream.codec));
            if (encoder)
            {
                cout << "Options for codec \"" << encoder->name() << "\":\n"
                     << "  " << encoder->getAvailableOptions() << "\n"
                     << "  Default: \"" << encoder->getDefaultOptions() << "\"\n";
            }
            exit(EXIT_SUCCESS);
        }

        AixLog::Log::init<AixLog::SinkNative>("snapserver", AixLog::Severity::trace, AixLog::Type::special);
        if (settings.logging.debug)
        {
            AixLog::Log::instance().add_logsink<AixLog::SinkCout>(AixLog::Severity::trace, AixLog::Type::all, "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)");
            if (!settings.logging.debug_logfile.empty())
                AixLog::Log::instance().add_logsink<AixLog::SinkFile>(AixLog::Severity::trace, AixLog::Type::all, settings.logging.debug_logfile,
                                                                      "%Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)");
        }
        else
        {
            AixLog::Log::instance().add_logsink<AixLog::SinkCout>(AixLog::Severity::info, AixLog::Type::all, "%Y-%m-%d %H-%M-%S [#severity]");
        }

        for (const auto& opt : conf.unknown_options())
            LOG(WARNING) << "unknown configuration option: " << opt << "\n";

        if (!streamValue->is_set())
            settings.stream.pcmStreams.push_back(streamValue->value());

        for (size_t n = 0; n < streamValue->count(); ++n)
        {
            LOG(INFO) << "Adding stream: " << streamValue->value(n) << "\n";
            settings.stream.pcmStreams.push_back(streamValue->value(n));
        }

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

        boost::asio::io_context io_context;
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
        auto publishZeroConfg = std::make_shared<PublishZeroConf>("Snapcast", io_context);
        vector<mDNSService> dns_services;
        dns_services.emplace_back("_snapcast._tcp", settings.stream.port);
        dns_services.emplace_back("_snapcast-stream._tcp", settings.stream.port);
        if (settings.tcp.enabled)
        {
            dns_services.emplace_back("_snapcast-jsonrpc._tcp", settings.tcp.port);
            dns_services.emplace_back("_snapcast-tcp._tcp", settings.tcp.port);
        }
        if (settings.http.enabled)
        {
            dns_services.emplace_back("_snapcast-http._tcp", settings.http.port);
        }
        publishZeroConfg->publish(dns_services);
#endif
        if (settings.stream.streamChunkMs < 10)
        {
            LOG(WARNING) << "Stream read chunk size is less than 10ms, changing to 10ms\n";
            settings.stream.streamChunkMs = 10;
        }

        if (settings.stream.bufferMs < 400)
        {
            LOG(WARNING) << "Buffer is less than 400ms, changing to 400ms\n";
            settings.stream.bufferMs = 400;
        }

        std::unique_ptr<StreamServer> streamServer(new StreamServer(io_context, settings));
        streamServer->start();

        if (num_threads < 0)
            num_threads = std::max(2, std::min(4, static_cast<int>(std::thread::hardware_concurrency())));
        LOG(INFO) << "number of threads: " << num_threads << ", hw threads: " << std::thread::hardware_concurrency() << "\n";

        auto sig = install_signal_handler({SIGHUP, SIGTERM, SIGINT},
                                          [&io_context](int signal, const std::string& name) {
                                              SLOG(INFO) << "Received signal " << signal << ": " << name << "\n";
                                              io_context.stop();
                                          });

        std::vector<std::thread> threads;
        for (int n = 0; n < num_threads; ++n)
            threads.emplace_back([&] { io_context.run(); });

        io_context.run();

        for (auto& t : threads)
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
    Config::instance().save();
    SLOG(NOTICE) << "daemon terminated." << endl;
    exit(exitcode);
}
