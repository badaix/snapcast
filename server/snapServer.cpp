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
#include "common/timeDefs.h"
#include "common/signalHandler.h"
#include "common/daemon.h"
#include "common/log.h"
#include "common/snapException.h"
#include "message/sampleFormat.h"
#include "message/message.h"
#include "pcmEncoder.h"
#include "oggEncoder.h"
#include "flacEncoder.h"
#include "controlServer.h"
#include "publishAvahi.h"


bool g_terminated = false;

namespace po = boost::program_options;

using namespace std;


std::condition_variable terminateSignaled;

int main(int argc, char* argv[])
{
	try
	{
		string sampleFormat;

		size_t port;
		string fifoName;
		string codec;
		bool runAsDaemon;
		int32_t bufferMs;

		po::options_description desc("Allowed options");
		desc.add_options()
		("help,h", "produce help message")
		("version,v", "show version number")
		("port,p", po::value<size_t>(&port)->default_value(98765), "server port")
		("sampleformat,s", po::value<string>(&sampleFormat)->default_value("44100:16:2"), "sample format")
		("codec,c", po::value<string>(&codec)->default_value("flac"), "transport codec [flac|ogg|pcm]")
		("fifo,f", po::value<string>(&fifoName)->default_value("/tmp/snapfifo"), "name of the input fifo file")
		("daemon,d", po::bool_switch(&runAsDaemon)->default_value(false), "daemonize")
		("buffer,b", po::value<int32_t>(&bufferMs)->default_value(1000), "buffer [ms]")
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
				 << "Copyright (C) 2014 BadAix (snapcast@badaix.de).\n"
				 << "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
				 << "This is free software: you are free to change and redistribute it.\n"
				 << "There is NO WARRANTY, to the extent permitted by law.\n\n"
				 << "Written by Johannes Pohl.\n";
			return 1;
		}

		std::clog.rdbuf(new Log("snapserver", LOG_DAEMON));

		msg::SampleFormat format(sampleFormat);
		std::unique_ptr<Encoder> encoder;
		if (codec == "ogg")
			encoder.reset(new OggEncoder(format));
		else if (codec == "pcm")
			encoder.reset(new PcmEncoder(format));
		else if (codec == "flac")
			encoder.reset(new FlacEncoder(format));
		else
		{
			cout << "unknown codec: " << codec << "\n";
			return 1;
		}

		umask(0);
		mkfifo(fifoName.c_str(), 0666);
		int fd = open(fifoName.c_str(), O_RDONLY | O_NONBLOCK);
		if (fd == -1)
		{
			cout << "failed to open fifo: " << fifoName << "\n";
			return 1;
		}

		msg::ServerSettings serverSettings;
		serverSettings.bufferMs = bufferMs;

		signal(SIGHUP, signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGINT, signal_handler);

		if (runAsDaemon)
		{
			daemonize("/var/run/snapserver.pid");
			logS(kLogNotice) << "daemon started." << endl;
		}

		ControlServer* controlServer = new ControlServer(port);
		controlServer->setServerSettings(&serverSettings);
		controlServer->setFormat(&format);
		controlServer->setHeader(encoder->getHeader());
		controlServer->start();

		PublishAvahi publishAvahi("SnapCast");
		std::vector<AvahiService> services;
		services.push_back(AvahiService("_snapcast._tcp", port));
		publishAvahi.publish(services);

		timeval tvChunk;
		gettimeofday(&tvChunk, NULL);
		long nextTick = chronos::getTickCount();
		size_t pcmReadMs = 20;
		
		while (!g_terminated)
		{
			try
			{
				shared_ptr<msg::PcmChunk> chunk;
				while (!g_terminated)//cin.good())
				{
					chunk.reset(new msg::PcmChunk(sampleFormat, pcmReadMs));
					int toRead = chunk->payloadSize;
					int len = 0;
					do
					{
						int count = read(fd, chunk->payload + len, toRead - len);
//continue;
						if (count <= 0)
							usleep(100*1000);
						else
							len += count;
					}
					while ((len < toRead) && !g_terminated);

					chunk->timestamp.sec = tvChunk.tv_sec;
					chunk->timestamp.usec = tvChunk.tv_usec;
					double chunkDuration = encoder->encode(chunk.get());
					if (chunkDuration > 0)
						controlServer->send(chunk);
//					logO << chunkDuration << "\n";
//                    addUs(tvChunk, 1000*chunk->getDuration());
					nextTick += pcmReadMs;
					chronos::addUs(tvChunk, chunkDuration * 1000);
					long currentTick = chronos::getTickCount();
					if (nextTick > currentTick)
					{
						usleep((nextTick - currentTick) * 1000);
					}	
					else
					{
						gettimeofday(&tvChunk, NULL);
						nextTick = chronos::getTickCount();
					}
				}
			}
			catch(const std::exception& e)
			{
				std::cerr << "Exception: " << e.what() << std::endl;
			}
			close(fd);
		}

	}
	catch (const std::exception& e)
	{
		logS(kLogErr) << "Exception: " << e.what() << std::endl;
	}

	logS(kLogNotice) << "daemon terminated." << endl;
	daemonShutdown();
}





