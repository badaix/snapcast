/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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
#ifdef HAS_DAEMON
#include "common/daemon.hpp"
#endif
#include "common/message/message.hpp"
#include "common/sample_format.hpp"
#include "common/snap_exception.hpp"
#include "common/time_defs.hpp"
#include "common/utils/string_utils.hpp"
#include "common/version.hpp"
#include "encoder/encoder_factory.hpp"
#include "server.hpp"
#include "server_settings.hpp"
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
#include "publishZeroConf/publish_mdns.hpp"
#endif
#include "common/aixlog.hpp"
#include "config.hpp"

// 3rd party headers
#include <boost/asio/ip/host_name.hpp>

// standard headers
#include <chrono>
#include <memory>
#include <sys/resource.h>


using namespace std;
using namespace popl;

static constexpr auto LOG_TAG = "Snapserver";


int main(int argc, char* argv[])
{
#ifdef MACOS
#pragma message "Warning: the macOS support is experimental and might not be maintained"
#endif

    int exitcode = EXIT_SUCCESS;
    try
    {
        ServerSettings settings;
        std::string pcmSource = "pipe:///tmp/snapfifo?name=default";
        std::string config_file = "/etc/snapserver.conf";

        OptionParser op("Allowed options");
        auto helpSwitch = op.add<Switch>("h", "help", "Produce help message, use -hh to show options from config file");
        auto groffSwitch = op.add<Switch, Attribute::hidden>("", "groff", "produce groff message");
        auto versionSwitch = op.add<Switch>("v", "version", "Show version number");
#ifdef HAS_DAEMON
        int processPriority(0);
        auto daemonOption = op.add<Implicit<int>>("d", "daemon", "Daemonize\noptional process priority [-20..19]", 0, &processPriority);
#endif
        op.add<Value<string>>("c", "config", "path to the configuration file", config_file, &config_file);

        OptionParser conf("Overridable config file options");

        // server settings
        conf.add<Value<int>>("", "server.threads", "number of server threads", settings.server.threads, &settings.server.threads);
        conf.add<Value<string>>("", "server.pidfile", "pid file when running as daemon", settings.server.pid_file, &settings.server.pid_file);
        conf.add<Value<string>>("", "server.user", "the user to run as when daemonized", settings.server.user, &settings.server.user);
        conf.add<Implicit<string>>("", "server.group", "the group to run as when daemonized", settings.server.group, &settings.server.group);
        conf.add<Implicit<string>>("", "server.datadir", "directory where persistent data is stored", settings.server.data_dir, &settings.server.data_dir);

        // HTTP RPC settings
        conf.add<Value<bool>>("", "http.enabled", "enable HTTP Json RPC (HTTP POST and websockets)", settings.http.enabled, &settings.http.enabled);
        conf.add<Value<size_t>>("", "http.port", "which port the server should listen on", settings.http.port, &settings.http.port);
        auto http_bind_to_address = conf.add<Value<string>>("", "http.bind_to_address", "address for the server to listen on",
                                                            settings.http.bind_to_address.front(), &settings.http.bind_to_address[0]);
        conf.add<Implicit<string>>("", "http.doc_root", "serve a website from the doc_root location", settings.http.doc_root, &settings.http.doc_root);
        conf.add<Value<string>>("", "http.host", "Hostname or IP under which clients can reach this host", settings.http.host, &settings.http.host);

        // TCP RPC settings
        conf.add<Value<bool>>("", "tcp.enabled", "enable TCP Json RPC)", settings.tcp.enabled, &settings.tcp.enabled);
        conf.add<Value<size_t>>("", "tcp.port", "which port the server should listen on", settings.tcp.port, &settings.tcp.port);
        auto tcp_bind_to_address = conf.add<Value<string>>("", "tcp.bind_to_address", "address for the server to listen on",
                                                           settings.tcp.bind_to_address.front(), &settings.tcp.bind_to_address[0]);

        // stream settings
        auto stream_bind_to_address = conf.add<Value<string>>("", "stream.bind_to_address", "address for the server to listen on",
                                                              settings.stream.bind_to_address.front(), &settings.stream.bind_to_address[0]);
        conf.add<Value<size_t>>("", "stream.port", "which port the server should listen on", settings.stream.port, &settings.stream.port);
        // deprecated: stream.stream, use stream.source instead
        auto streamValue = conf.add<Value<string>>("", "stream.stream", "Deprecated: use stream.source", pcmSource, &pcmSource);
        auto sourceValue = conf.add<Value<string>>(
            "", "stream.source", "URI of the PCM input stream.\nFormat: TYPE://host/path?name=NAME\n[&codec=CODEC]\n[&sampleformat=SAMPLEFORMAT]", pcmSource,
            &pcmSource);

        conf.add<Value<string>>("", "stream.sampleformat", "Default sample format", settings.stream.sampleFormat, &settings.stream.sampleFormat);
        conf.add<Value<string>>("", "stream.codec", "Default transport codec\n(flac|ogg|opus|pcm)[:options]\nType codec:? to get codec specific options",
                                settings.stream.codec, &settings.stream.codec);
        // deprecated: stream_buffer, use chunk_ms instead
        conf.add<Value<size_t>>("", "stream.stream_buffer", "Default stream read chunk size [ms], deprecated, use stream.chunk_ms instead",
                                settings.stream.streamChunkMs, &settings.stream.streamChunkMs);
        conf.add<Value<size_t>>("", "stream.chunk_ms", "Default stream read chunk size [ms]", settings.stream.streamChunkMs, &settings.stream.streamChunkMs);
        conf.add<Value<int>>("", "stream.buffer", "Buffer [ms]", settings.stream.bufferMs, &settings.stream.bufferMs);
        conf.add<Value<bool>>("", "stream.send_to_muted", "Send audio to muted clients", settings.stream.sendAudioToMutedClients,
                              &settings.stream.sendAudioToMutedClients);

        // streaming_client options
        conf.add<Value<uint16_t>>("", "streaming_client.initial_volume", "Volume [percent] assigned to new streaming clients",
                                  settings.streamingclient.initialVolume, &settings.streamingclient.initialVolume);

        // logging settings
        conf.add<Value<string>>("", "logging.sink", "log sink [null,system,stdout,stderr,file:<filename>]", settings.logging.sink, &settings.logging.sink);
        auto logfilterOption = conf.add<Value<string>>(
            "", "logging.filter",
            "log filter <tag>:<level>[,<tag>:<level>]* with tag = * or <log tag> and level = [trace,debug,info,notice,warning,error,fatal]",
            settings.logging.filter);

        try
        {
            op.parse(argc, argv);
            conf.parse(config_file);
            conf.parse(argc, argv);
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
            cerr << "Exception: " << e.what() << std::endl;
            cout << "\n" << op << "\n";
            exit(EXIT_FAILURE);
        }

        if (versionSwitch->is_set())
        {
            cout << "snapserver v" << version::code << (!version::rev().empty() ? (" (rev " + version::rev(8) + ")") : ("")) << "\n"
                 << "Copyright (C) 2014-2022 BadAix (snapcast@badaix.de).\n"
                 << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
                 << "This is free software: you are free to change and redistribute it.\n"
                 << "There is NO WARRANTY, to the extent permitted by law.\n\n"
                 << "Written by Johannes M. Pohl and contributors <https://github.com/badaix/snapcast/graphs/contributors>.\n\n";
            exit(EXIT_SUCCESS);
        }

        if (helpSwitch->is_set())
        {
            cout << op << "\n";
            if (helpSwitch->count() > 1)
                cout << conf << "\n";
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
            AixLog::Log::init<AixLog::SinkNative>("snapserver", logfilter);
        else if (settings.logging.sink == "null")
            AixLog::Log::init<AixLog::SinkNull>();
        else
            throw SnapException("Invalid log sink: " + settings.logging.sink);

        LOG(INFO, LOG_TAG) << "Version " << version::code << (!version::rev().empty() ? (", revision " + version::rev(8)) : ("")) << "\n";

        if (!streamValue->is_set() && !sourceValue->is_set())
            settings.stream.sources.push_back(sourceValue->value());

        for (size_t n = 0; n < streamValue->count(); ++n)
        {
            LOG(INFO, LOG_TAG) << "Adding stream: " << streamValue->value(n) << "\n";
            settings.stream.sources.push_back(streamValue->value(n));
        }
        for (size_t n = 0; n < sourceValue->count(); ++n)
        {
            LOG(INFO, LOG_TAG) << "Adding source: " << sourceValue->value(n) << "\n";
            settings.stream.sources.push_back(sourceValue->value(n));
        }

#ifdef HAS_DAEMON
        std::unique_ptr<Daemon> daemon;
        if (daemonOption->is_set())
        {
            if (settings.server.user.empty())
                throw std::invalid_argument("user must not be empty");

            if (settings.server.data_dir.empty())
                settings.server.data_dir = "/var/lib/snapserver";
            Config::instance().init(settings.server.data_dir, settings.server.user, settings.server.group);

            daemon = std::make_unique<Daemon>(settings.server.user, settings.server.group, settings.server.pid_file);
            processPriority = std::min(std::max(-20, processPriority), 19);
            if (processPriority != 0)
                setpriority(PRIO_PROCESS, 0, processPriority);
            LOG(NOTICE, LOG_TAG) << "daemonizing" << std::endl;
            daemon->daemonize();
            LOG(NOTICE, LOG_TAG) << "daemon started" << std::endl;
        }
        else
            Config::instance().init(settings.server.data_dir);
#else
        Config::instance().init(settings.server.data_dir);
#endif

        boost::asio::io_context io_context;
#if defined(HAS_AVAHI) || defined(HAS_BONJOUR)
        auto publishZeroConfg = std::make_unique<PublishZeroConf>("Snapcast", io_context);
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
            if ((settings.http.host == "<hostname>") || settings.http.host.empty())
            {
                settings.http.host = boost::asio::ip::host_name();
                LOG(INFO, LOG_TAG) << "Using HTTP host name: " << settings.http.host << "\n";
            }
        }
        publishZeroConfg->publish(dns_services);
#endif
        if (settings.stream.streamChunkMs < 10)
        {
            LOG(WARNING, LOG_TAG) << "Stream read chunk size is less than 10ms, changing to 10ms\n";
            settings.stream.streamChunkMs = 10;
        }

        static constexpr chrono::milliseconds MIN_BUFFER_DURATION = 20ms;
        if (settings.stream.bufferMs < MIN_BUFFER_DURATION.count())
        {
            LOG(WARNING, LOG_TAG) << "Buffer is less than " << MIN_BUFFER_DURATION.count() << "ms, changing to " << MIN_BUFFER_DURATION.count() << "ms\n";
            settings.stream.bufferMs = MIN_BUFFER_DURATION.count();
        }

        auto server = std::make_unique<Server>(io_context, settings);
        server->start();

        if (settings.server.threads < 0)
            settings.server.threads = std::max(2, std::min(4, static_cast<int>(std::thread::hardware_concurrency())));
        LOG(INFO, LOG_TAG) << "Number of threads: " << settings.server.threads << ", hw threads: " << std::thread::hardware_concurrency() << "\n";

        // Construct a signal set registered for process termination.
        boost::asio::signal_set signals(io_context, SIGHUP, SIGINT, SIGTERM);
        signals.async_wait(
            [&io_context](const boost::system::error_code& ec, int signal)
            {
            if (!ec)
                LOG(INFO, LOG_TAG) << "Received signal " << signal << ": " << strsignal(signal) << "\n";
            else
                LOG(INFO, LOG_TAG) << "Failed to wait for signal, error: " << ec.message() << "\n";
            io_context.stop();
        });

        std::vector<std::thread> threads;
        for (int n = 0; n < settings.server.threads; ++n)
            threads.emplace_back([&] { io_context.run(); });

        io_context.run();

        for (auto& t : threads)
            t.join();

        LOG(INFO, LOG_TAG) << "Stopping streamServer" << endl;
        server->stop();
        LOG(INFO, LOG_TAG) << "done" << endl;
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << std::endl;
        exitcode = EXIT_FAILURE;
    }
    Config::instance().save();
    LOG(NOTICE, LOG_TAG) << "Snapserver terminated." << endl;
    exit(exitcode);
}
