#include <boost/program_options.hpp>
#include <chrono>
#include <memory>
#include "common/timeDefs.h"
#include "common/signalHandler.h"
#include "common/utils.h"
#include "message/sampleFormat.h"
#include "message/message.h"
#include "pcmEncoder.h"
#include "oggEncoder.h"
#include "controlServer.h"


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
		("port,p", po::value<size_t>(&port)->default_value(98765), "server port")
		("sampleformat,s", po::value<string>(&sampleFormat)->default_value("48000:16:2"), "sample format")
		("codec,c", po::value<string>(&codec)->default_value("ogg"), "transport codec [ogg|pcm]")
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


		if (runAsDaemon)
		{
			daemonize();
			syslog (LOG_NOTICE, "First daemon started.");
		}

		openlog ("firstdaemon", LOG_PID, LOG_DAEMON);

		using namespace std; // For atoi.

		timeval tvChunk;
		gettimeofday(&tvChunk, NULL);
		long nextTick = chronos::getTickCount();

		mkfifo(fifoName.c_str(), 0777);
		msg::SampleFormat format(sampleFormat);
		size_t duration = 50;
//size_t chunkSize = duration*format.rate*format.frameSize / 1000;
		std::auto_ptr<Encoder> encoder;
		if (codec == "ogg")
			encoder.reset(new OggEncoder(sampleFormat));
		else if (codec == "pcm")
			encoder.reset(new PcmEncoder(sampleFormat));
		else if (codec == "flac")
		{
			cout << "Not yet supported\n";
			return 1;
		}
		else
		{
			cout << "unknown codec: " << codec << "\n";
			return 1;
		}

		msg::ServerSettings serverSettings;
		serverSettings.bufferMs = bufferMs;

		ControlServer* controlServer = new ControlServer(port);
		controlServer->setServerSettings(&serverSettings);
		controlServer->setFormat(&format);
		controlServer->setHeader(encoder->getHeader());
		controlServer->start();

		signal(SIGHUP, signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGINT, signal_handler);

		while (!g_terminated)
		{
			int fd = open(fifoName.c_str(), O_RDONLY | O_NONBLOCK);
			try
			{
				shared_ptr<msg::PcmChunk> chunk;//(new WireChunk());
				while (!g_terminated)//cin.good())
				{
					chunk.reset(new msg::PcmChunk(sampleFormat, duration));//2*WIRE_CHUNK_SIZE));
					int toRead = chunk->payloadSize;
					int len = 0;
					do
					{
						int count = read(fd, chunk->payload + len, toRead - len);
						if (count == 0)
							throw ServerException("count = 0");
						else if (count == -1)
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
//cout << chunk->tv_sec << ", " << chunk->tv_usec / 1000 << "\n";
//                    addUs(tvChunk, 1000*chunk->getDuration());
					chronos::addUs(tvChunk, chunkDuration * 1000);
					nextTick += duration;
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

//		server->stop();
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
	}

	syslog (LOG_NOTICE, "First daemon terminated.");
	cout << "Terminated\n";
	closelog();
}





