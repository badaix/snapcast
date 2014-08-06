//
// blocking_tcp_echo_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2010 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include <chrono>
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip>


using boost::asio::ip::tcp;
using namespace std;
using namespace std::chrono;


enum { max_length = 1024 };


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

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
		return 1;
	}
	try
	{
		boost::asio::io_service io_service;
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(tcp::v4(), argv[1], argv[2]);
		tcp::resolver::iterator iterator = resolver.resolve(query);

		while (true)
		{
			try
			{
				tcp::socket s(io_service);
				s.connect(*iterator);
				boost::array<char, 128> buf;
				boost::system::error_code error;

				while (true)
				{
					size_t len = s.read_some(boost::asio::buffer(buf), error);
					if (error == boost::asio::error::eof)
						break;

					std::cout.write(buf.data(), len);
					std::cout.flush();
				}
			}
			catch (std::exception& e)
			{
				std::cerr << "Exception: " << e.what() << "\n";
				usleep(100*1000);
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

  return 0;
}


