#ifndef RECEIVER_H
#define RECEIVER_H

#include <string>
#include <thread>
#include <atomic>
#include <boost/asio.hpp>
#include "stream.h"

using boost::asio::ip::tcp;


class Receiver
{
public:
	Receiver(Stream* stream);
	void start(const std::string& ip, int port);
	void stop();

private:
	void socketRead(tcp::socket* socket, void* to, size_t bytes);
	void worker();
	boost::asio::io_service io_service;
	tcp::resolver::iterator iterator;
	std::atomic<bool> active_;
	Stream* stream_;
	std::thread* receiverThread;
};


#endif

