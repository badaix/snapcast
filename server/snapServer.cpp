//
// blocking_tcp_echo_server.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <vector>
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip>
#include <thread>
#include <memory>
#include <set>
#include "common/chunk.h"
#include "common/timeUtils.h"
#include "common/queue.h"


using boost::asio::ip::tcp;
namespace po = boost::program_options;

const int max_length = 1024;

typedef boost::shared_ptr<tcp::socket> socket_ptr;
using namespace std;
using namespace std::chrono;


std::string return_current_time_and_date()
{
    auto now = system_clock::now();
    auto in_time_t = system_clock::to_time_t(now);
	system_clock::duration ms = now.time_since_epoch();
	char buff[20];
	strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&in_time_t));
	stringstream ss;
	ss << buff << "." << std::setw(3) << std::setfill('0') << ((ms / milliseconds(1)) % 1000);
    return ss.str();
}


class Session
{
public:
	Session(socket_ptr sock) : active_(false), socket_(sock)
	{
	}

	void sender()
	{
		try
		{
			for (;;)
			{
				shared_ptr<WireChunk> chunk(chunks.pop());
				size_t written = 0;
				do
				{
					written += boost::asio::write(*socket_, boost::asio::buffer(chunk.get() + written, sizeof(WireChunk) - written));//, error);
				}
				while (written < sizeof(WireChunk));
			}
		}
		catch (std::exception& e)
		{
			std::cerr << "Exception in thread: " << e.what() << "\n";
			active_ = false;
		}
	}

	void start()
	{
		active_ = true;
		senderThread = new thread(&Session::sender, this);
//		readerThread.join();
	}

	void send(shared_ptr<WireChunk> chunk)
	{
		while (chunks.size() * WIRE_CHUNK_MS > 10000)
			chunks.pop();
		chunks.push(chunk);
	}

	bool isActive() const
	{
		return active_;
	}

private:
	bool active_;
	socket_ptr socket_;
	thread* senderThread;
	Queue<shared_ptr<WireChunk>> chunks;
};


class Server
{
public:
	Server(unsigned short port) : port_(port)
	{
	}

	void acceptor()
	{
		tcp::acceptor a(io_service_, tcp::endpoint(tcp::v4(), port_));
		for (;;)
		{
			socket_ptr sock(new tcp::socket(io_service_));
			a.accept(*sock);
			cout << "New connection: " << sock->remote_endpoint().address().to_string() << "\n";
			Session* session = new Session(sock);
			session->start();
			sessions.insert(shared_ptr<Session>(session));
		}
	}

	void send(shared_ptr<WireChunk> chunk)
	{
		for (std::set<shared_ptr<Session>>::iterator it = sessions.begin(); it != sessions.end(); ) 
		{
    		if (!(*it)->isActive())
			{
				cout << "Session inactive. Removing\n";
		        sessions.erase(it++);
			}
		    else
		        ++it;
	    }

		for (auto s : sessions)
			s->send(chunk);
	}

	void start()
	{
		acceptThread = new thread(&Server::acceptor, this);
	}

	void stop()
	{
		acceptThread->join();
	}

private:
	set<shared_ptr<Session>> sessions;
	boost::asio::io_service io_service_;
	unsigned short port_;
	thread* acceptThread;
};


int main(int argc, char* argv[])
{
	try
	{
        size_t port;
        string fifoName;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help,h", "produce help message")
            ("port,p", po::value<size_t>(&port)->default_value(98765), "port to listen on")
            ("fifo,f", po::value<string>(&fifoName)->default_value("/tmp/snapfifo"), "name of fifo file")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            cout << desc << "\n";
            return 1;
        }

/*        if (vm.count("port") == 0)
        {
            cout << "Please specify server port\n";
            return 1;
        }

//        cout << "Compression level was set to " << vm["compression"].as<int>() << ".\n";

		if (argc == 1)
		{
			std::cerr << desc << "\n";
			return 1;
		}
*/

		using namespace std; // For atoi.
		Server* server = new Server(port);
		server->start();

		timeval tvChunk;
		gettimeofday(&tvChunk, NULL);
		long nextTick = getTickCount();

/*        pid_t pid, sid;
        
        pid = fork();
        if (pid < 0) {
                exit(EXIT_FAILURE);
        }
        if (pid > 0) {
                exit(EXIT_SUCCESS);
        }

        umask(0);
                
        sid = setsid();
        if (sid < 0) {
                exit(EXIT_FAILURE);
        }
        
        if ((chdir("/")) < 0) {
                exit(EXIT_FAILURE);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
 */       
        mkfifo(fifoName.c_str(), 0777);
        while (true)
        {
            int fd = open(fifoName.c_str(), O_RDONLY);
            try
            {
                shared_ptr<WireChunk> chunk;//(new WireChunk());
                while (true)//cin.good())
                {
                    chunk.reset(new WireChunk());
                    int toRead = sizeof(WireChunk::payload);
//                    cout << "tr: " << toRead << ", size: " << WIRE_CHUNK_SIZE << "\t";
                    char* payload = (char*)(&chunk->payload[0]);
                    int len = 0;
                    do
                    {
                        int count = read(fd, payload + len, toRead - len);
                        cout.flush();
                        if (count <= 0)
                            throw new std::exception();
                        len += count;
                    }
                    while (len < toRead);

                    chunk->tv_sec = tvChunk.tv_sec;
                    chunk->tv_usec = tvChunk.tv_usec;
                    server->send(chunk);

                    addMs(tvChunk, WIRE_CHUNK_MS);
                    nextTick += WIRE_CHUNK_MS;
                    long currentTick = getTickCount();
                    if (nextTick > currentTick)
                    {
                        usleep((nextTick - currentTick) * 1000);
                    }
                    else
                    {
                        cin.sync();
                        gettimeofday(&tvChunk, NULL);
                        nextTick = getTickCount();
                    }
                }
            }
            catch(const std::exception&)
            {
                cout << "Exception\n";
            }
            close(fd);
        }

		return 0;
		server->stop();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}



