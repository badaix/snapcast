/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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
#include <algorithm>
#include <cstddef>
#include <filesystem>
#ifdef HAS_DAEMON
#include "common/daemon.hpp"
#endif
#include "common/snap_exception.hpp"
#include "common/utils/string_utils.hpp"
#include "common/version.hpp"
#include "encoder/encoder_factory.hpp"
#include "server.hpp"
#include "server_settings.hpp"
#ifdef HAS_MDNS
#include "publishZeroConf/publish_zeroconf.hpp"
#endif
#include "common/aixlog.hpp"
#include "config.hpp"

// 3rd party headers
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/signal_set.hpp>

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
        auto config_file_option = op.add<Value<string>>("c", "config", "Path to the configuration file", config_file, &config_file);

        OptionParser conf("Overridable config file options");

        // server settings
        conf.add<Value<int>>("", "server.threads", "number of server threads", settings.server.threads, &settings.server.threads);
        conf.add<Value<string>>("", "server.pidfile", "pid file when running as daemon", settings.server.pid_file, &settings.server.pid_file);
        conf.add<Value<string>>("", "server.user", "the user to run as when daemonized", settings.server.user, &settings.server.user);
        conf.add<Implicit<string>>("", "server.group", "the group to run as when daemonized", settings.server.group, &settings.server.group);
        conf.add<Implicit<string>>("", "server.datadir", "directory where persistent data is stored", settings.server.data_dir, &settings.server.data_dir);
        conf.add<Value<bool>>("", "server.mdns_enabled", "enable mDNS to publish services", settings.server.mdns_enabled, &settings.server.mdns_enabled);

        // SSL settings
        conf.add<Value<std::filesystem::path>>("", "ssl.certificate", "certificate file (PEM format)", settings.ssl.certificate, &settings.ssl.certificate);
        conf.add<Value<std::filesystem::path>>("", "ssl.certificate_key", "private key file (PEM format)", settings.ssl.certificate_key,
                                               &settings.ssl.certificate_key);
        conf.add<Value<string>>("", "ssl.key_password", "key password (for encrypted private key)", settings.ssl.key_password, &settings.ssl.key_password);
        conf.add<Value<bool>>("", "ssl.verify_clients", "Verify client certificates", settings.ssl.verify_clients, &settings.ssl.verify_clients);
        auto client_cert_opt =
            conf.add<Value<std::filesystem::path>>("", "ssl.client_cert", "List of client CA certificate files, can be configured multiple times", "");

        // Users setting
        conf.add<Value<bool>>("", "authorization.enabled", "enabled authorization", settings.auth.enabled, &settings.auth.enabled);
        auto roles_value = conf.add<Value<string>>("", "authorization.role", "<Role name>:<permissions>");
        auto users_value = conf.add<Value<string>>("", "authorization.user", "<User nane>:<password>:<role>");

        // HTTP RPC settings
        conf.add<Value<bool>>("", "http.enabled", "enable HTTP Json RPC (HTTP POST and websockets)", settings.http.enabled, &settings.http.enabled);
        conf.add<Value<size_t>>("", "http.port", "which port the server should listen on", settings.http.port, &settings.http.port);
        auto http_bind_to_address = conf.add<Value<string>>("", "http.bind_to_address", "address for the server to listen on",
                                                            settings.http.bind_to_address.front(), &settings.http.bind_to_address[0]);
        conf.add<Value<bool>>("", "http.ssl_enabled", "enable HTTPS Json RPC (HTTPS POST and ssl websockets)", settings.http.ssl_enabled,
                              &settings.http.ssl_enabled);
        conf.add<Value<size_t>>("", "http.ssl_port", "which ssl port the server should listen on", settings.http.ssl_port, &settings.http.ssl_port);
        auto http_ssl_bind_to_address = conf.add<Value<string>>("", "http.ssl_bind_to_address", "ssl address for the server to listen on",
                                                                settings.http.ssl_bind_to_address.front(), &settings.http.ssl_bind_to_address[0]);
        conf.add<Implicit<string>>("", "http.doc_root", "serve a website from the doc_root location", settings.http.doc_root, &settings.http.doc_root);
        conf.add<Value<string>>("", "http.host", "Hostname or IP under which clients can reach this host", settings.http.host, &settings.http.host);
        conf.add<Value<string>>("", "http.url_prefix", "URL prefix for generating album art URLs", settings.http.url_prefix, &settings.http.url_prefix);
        conf.add<Value<bool>>("", "http.publish_http", "Publish HTTP service via mDNS", settings.http.publish_http, &settings.http.publish_http);
        conf.add<Value<bool>>("", "http.publish_https", "Publish HTTPS service via mDNS", settings.http.publish_https, &settings.http.publish_https);

        // TCP RPC settings
        // Deprecated: tcp.x => tcp-control.x
        auto deprecated_tcp_enabled =
            conf.add<Value<bool>>("", "tcp.enabled", "deprecated: use 'tcp-control.enabled'", settings.tcp_control.enabled, &settings.tcp_control.enabled);
        auto deprecated_tcp_port =
            conf.add<Value<size_t>>("", "tcp.port", "deprecated: use 'tcp-control.port'", settings.tcp_control.port, &settings.tcp_control.port);
        auto deprecated_tcp_bind_to_address = conf.add<Value<string>>("", "tcp.bind_to_address", "deprecated: use 'tcp-control.bind_to_address'",
                                                                      settings.tcp_control.bind_to_address.front(), &settings.tcp_control.bind_to_address[0]);

        // Deprecated: stream.x => tcp-streaming.x
        auto deprecated_stream_bind_to_address = conf.add<Value<string>>("", "stream.bind_to_address", "deprecated: use 'tcp-streaming.bind_to_address'",
                                                                         settings.tcp_stream.bind_to_address.front(), &settings.tcp_stream.bind_to_address[0]);
        auto deprecated_stream_port =
            conf.add<Value<size_t>>("", "stream.port", "deprecated: use 'tcp-streaming.port'", settings.tcp_stream.port, &settings.tcp_stream.port);


        conf.add<Value<bool>>("", "tcp-control.enabled", "enable TCP Json RPC)", settings.tcp_control.enabled, &settings.tcp_control.enabled);
        conf.add<Value<size_t>>("", "tcp-control.port", "which control port the server should listen on", settings.tcp_control.port,
                                &settings.tcp_control.port);
        auto control_bind_to_address = conf.add<Value<string>>("", "tcp-control.bind_to_address", "address for the control server to listen on",
                                                               settings.tcp_control.bind_to_address.front(), &settings.tcp_control.bind_to_address[0]);
        conf.add<Value<bool>>("", "tcp-control.publish", "Publish TCP control service via mDNS", settings.tcp_control.publish, &settings.tcp_control.publish);

        // TCP Streaming settings
        conf.add<Value<bool>>("", "tcp-streaming.enabled", "enable TCP streaming)", settings.tcp_stream.enabled, &settings.tcp_stream.enabled);
        conf.add<Value<size_t>>("", "tcp-streaming.port", "which streaming port the server should listen on", settings.tcp_stream.port,
                                &settings.tcp_stream.port);
        auto stream_bind_to_address = conf.add<Value<string>>("", "tcp-streaming.bind_to_address", "address for the streaming server to listen on",
                                                              settings.tcp_stream.bind_to_address.front(), &settings.tcp_stream.bind_to_address[0]);
        conf.add<Value<bool>>("", "tcp-streaming.publish", "Publish TCP streaming service via mDNS", settings.tcp_stream.publish, &settings.tcp_stream.publish);

        // stream settings
        conf.add<Value<std::filesystem::path>>("", "stream.plugin_dir", "stream plugin directory", settings.stream.plugin_dir, &settings.stream.plugin_dir);
        conf.add<Value<std::filesystem::path>>("", "stream.sandbox_dir", "directory with executable process stream sources", settings.stream.sandbox_dir,
                                               &settings.stream.sandbox_dir);
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

        // Parse command line arguments
        try
        {
            op.parse(argc, argv);
        }
        catch (const std::invalid_argument& e)
        {
            cerr << "Exception: " << e.what() << "\n";
            cout << "\n" << op << "\n";
            exit(EXIT_FAILURE);
        }

        if (versionSwitch->is_set())
        {
            cout << "snapserver v" << version::code << (!version::rev().empty() ? (" (rev " + version::rev(8) + ")") : ("")) << "\n"
                 << "Copyright (C) 2014-2025 BadAix (snapcast@badaix.de).\n"
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

        // Parse configuration file and overrides from command line
        try
        {
            conf.parse(config_file);
            conf.parse(argc, argv);
        }
        catch (const std::invalid_argument& e)
        {
            if (config_file_option->is_set())
            {
                cerr << "Exception: " << e.what() << "\n";
                cout << "\n" << op << "\n";
                exit(EXIT_FAILURE);
            }
            else
                cerr << "Warning - Failed to load config file '" << config_file << "': " << e.what() << "\n";
        }

        if (settings.stream.codec.find(":?") != string::npos)
        {
            encoder::EncoderFactory encoderFactory;
            std::unique_ptr<encoder::Encoder> encoder(encoderFactory.createEncoder(settings.stream.codec));
            if (encoder)
            {
                cout << "Options for codec '" << encoder->name() << "':\n"
                     << "  " << encoder->getAvailableOptions() << "\n"
                     << "  Default: '" << encoder->getDefaultOptions() << "'\n";
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

#ifndef HAS_OPENSSL
        if (settings.http.ssl_enabled)
            throw SnapException("HTTPS enabled ([http] ssl_enabled), but snapserrver is built without ssl support");
#endif

        if (deprecated_tcp_enabled->is_set())
            LOG(WARNING, LOG_TAG) << "Option '" << deprecated_tcp_enabled->long_name() << "' is " << deprecated_tcp_enabled->description() << "\n";
        if (deprecated_tcp_port->is_set())
            LOG(WARNING, LOG_TAG) << "Option '" << deprecated_tcp_port->long_name() << "' is " << deprecated_tcp_port->description() << "\n";
        if (deprecated_tcp_bind_to_address->is_set())
        {
            LOG(WARNING, LOG_TAG) << "Option '" << deprecated_tcp_bind_to_address->long_name() << "' is " << deprecated_tcp_bind_to_address->description()
                                  << "\n";
            if (!control_bind_to_address->is_set())
                control_bind_to_address = deprecated_tcp_bind_to_address;
        }
        if (deprecated_stream_bind_to_address->is_set())
        {
            LOG(WARNING, LOG_TAG) << "Option '" << deprecated_stream_bind_to_address->long_name() << "' is " << deprecated_stream_bind_to_address->description()
                                  << "\n";
            if (!stream_bind_to_address->is_set())
                stream_bind_to_address = deprecated_stream_bind_to_address;
        }
        if (deprecated_stream_port->is_set())
            LOG(WARNING, LOG_TAG) << "Option '" << deprecated_stream_port->long_name() << "' is " << deprecated_stream_port->description() << "\n";

        if (control_bind_to_address->is_set())
        {
            settings.tcp_control.bind_to_address.clear();
            for (size_t n = 0; n < control_bind_to_address->count(); ++n)
                settings.tcp_control.bind_to_address.push_back(control_bind_to_address->value(n));
        }
        if (http_bind_to_address->is_set())
        {
            settings.http.bind_to_address.clear();
            for (size_t n = 0; n < http_bind_to_address->count(); ++n)
                settings.http.bind_to_address.push_back(http_bind_to_address->value(n));
        }
        if (http_ssl_bind_to_address->is_set())
        {
            settings.http.ssl_bind_to_address.clear();
            for (size_t n = 0; n < http_ssl_bind_to_address->count(); ++n)
                settings.http.ssl_bind_to_address.push_back(http_ssl_bind_to_address->value(n));
        }
        if (stream_bind_to_address->is_set())
        {
            settings.tcp_stream.bind_to_address.clear();
            for (size_t n = 0; n < stream_bind_to_address->count(); ++n)
                settings.tcp_stream.bind_to_address.push_back(stream_bind_to_address->value(n));
        }

        if (!settings.ssl.certificate.empty() && !settings.ssl.certificate_key.empty())
        {
            namespace fs = std::filesystem;
            auto make_absolute = [](const fs::path& filename)
            {
                const fs::path cert_path = "/etc/snapserver/certs/";
                if (filename.is_absolute())
                    return filename;
                if (fs::exists(filename))
                    return fs::canonical(filename);
                return cert_path / filename;
            };
            settings.ssl.certificate = make_absolute(settings.ssl.certificate);
            if (!fs::exists(settings.ssl.certificate))
                throw SnapException("SSL certificate file not found: " + settings.ssl.certificate.native());
            settings.ssl.certificate_key = make_absolute(settings.ssl.certificate_key);
            if (!fs::exists(settings.ssl.certificate_key))
                throw SnapException("SSL certificate_key file not found: " + settings.ssl.certificate_key.native());
            for (size_t n = 0; n < client_cert_opt->count(); ++n)
            {
                auto cert_file = std::filesystem::weakly_canonical(client_cert_opt->value(n));
                if (!fs::exists(cert_file))
                    throw SnapException("Client certificate file not found: " + cert_file.string());
                settings.ssl.client_certs.push_back(std::move(cert_file));
            }
        }
        else if (settings.ssl.certificate.empty() != settings.ssl.certificate_key.empty())
        {
            throw SnapException("Both SSL 'certificate' and 'certificate_key' must be set or empty");
        }

        if (!settings.ssl.enabled())
        {
            if (settings.http.ssl_enabled)
                throw SnapException("HTTPS enabled ([http] ssl_enabled), but no certificates specified");
        }

        LOG(INFO, LOG_TAG) << "Version " << version::code << (!version::rev().empty() ? (", revision " + version::rev(8)) : ("")) << "\n";

        if (settings.ssl.enabled())
            LOG(INFO, LOG_TAG) << "SSL enabled - certificate file: '" << settings.ssl.certificate.native() << "', certificate key file: '"
                               << settings.ssl.certificate_key.native() << "'\n";

        if (!streamValue->is_set() && !sourceValue->is_set())
            settings.stream.sources.push_back(sourceValue->value());

        settings.stream.plugin_dir = std::filesystem::weakly_canonical(settings.stream.plugin_dir);
        settings.stream.sandbox_dir = std::filesystem::weakly_canonical(settings.stream.sandbox_dir);
        LOG(INFO, LOG_TAG) << "Stream plugin directory: '" << settings.stream.plugin_dir << "', sandbox directory: '" << settings.stream.sandbox_dir << "'\n";
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

        // TODO: auth
        settings.auth.enabled = false;
        if (settings.auth.enabled)
        {
            std::vector<std::string> roles;
            roles.reserve(roles_value->count());
            for (size_t n = 0; n < roles_value->count(); ++n)
                roles.push_back(roles_value->value(n));
            std::vector<std::string> users;
            users.reserve(users_value->count());
            for (size_t n = 0; n < users_value->count(); ++n)
                users.push_back(users_value->value(n));

            settings.auth.init(roles, users);

            for (const auto& role : settings.auth.roles)
                LOG(DEBUG, LOG_TAG) << "Role: " << role->role << ", permissions: " << utils::string::container_to_string(role->permissions) << "\n";
            for (const auto& user : settings.auth.users)
                LOG(DEBUG, LOG_TAG) << "User: " << user.name << ", pw: " << user.password << ", role: " << user.role_name << "\n";
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
            LOG(NOTICE, LOG_TAG) << "daemonizing" << "\n";
            daemon->daemonize();
            LOG(NOTICE, LOG_TAG) << "daemon started" << "\n";
        }
        else
            Config::instance().init(settings.server.data_dir);
#else
        Config::instance().init(settings.server.data_dir);
#endif

        boost::asio::io_context io_context;
#ifdef HAS_MDNS
        std::unique_ptr<PublishZeroConf> publishZeroConfg;
        if (settings.server.mdns_enabled)
        {
            publishZeroConfg = std::make_unique<PublishZeroConf>("Snapcast", io_context);
            vector<mDNSService> dns_services;
            if (settings.tcp_stream.enabled && settings.tcp_stream.publish)
            {
                dns_services.emplace_back("_snapcast._tcp", settings.tcp_stream.port);
                // dns_services.emplace_back("_snapcast-stream._tcp", settings.tcp_stream.port);
            }
            if (settings.tcp_control.enabled && settings.tcp_control.publish)
            {
                dns_services.emplace_back("_snapcast-ctrl._tcp", settings.tcp_control.port);
            }
            if (settings.http.enabled && settings.http.publish_http)
            {
                dns_services.emplace_back("_snapcast-http._tcp", settings.http.port);
            }
            if (settings.http.ssl_enabled && settings.http.publish_https)
            {
                dns_services.emplace_back("_snapcast-https._tcp", settings.http.ssl_port);
            }

            publishZeroConfg->publish(dns_services);
        }
#endif
        if (settings.http.enabled || settings.http.ssl_enabled)
        {
            if ((settings.http.host == "<hostname>") || settings.http.host.empty())
            {
                settings.http.host = boost::asio::ip::host_name();
                LOG(INFO, LOG_TAG) << "Using HTTP host name: " << settings.http.host << "\n";
            }
        }

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
        signals.async_wait([&io_context](const boost::system::error_code& ec, int signal)
        {
            if (!ec)
                LOG(INFO, LOG_TAG) << "Received signal " << signal << ": " << strsignal(signal) << "\n";
            else
                LOG(INFO, LOG_TAG) << "Failed to wait for signal, error: " << ec.message() << "\n";
            io_context.stop();
        });

        std::vector<std::thread> threads;
        threads.reserve(settings.server.threads);
        for (int n = 0; n < settings.server.threads; ++n)
            threads.emplace_back([&] { io_context.run(); });

        io_context.run();

        for (auto& t : threads)
            t.join();

        LOG(INFO, LOG_TAG) << "Stopping streamServer\n";
        server->stop();
        LOG(INFO, LOG_TAG) << "done\n";
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << "\n";
        exitcode = EXIT_FAILURE;
    }
    Config::instance().save();
    LOG(NOTICE, LOG_TAG) << "Snapserver terminated.\n";
    exit(exitcode);
}
