//
//  Custom routing Router to Mama (ROUTER to REQ)
//
// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>

#include <thread> 
#include <istream>
#include "zhelpers.hpp"
#include "utils.h"

zmq::socket_t* client;

void receiver(zmq::socket_t* client)
{
	while (true)
	{
		std::string address = s_recv (*client);
	    // receiving and discarding'empty' message
	    s_recv (*client);
	    // receiving and discarding 'ready' message
	    std::string msg = s_recv (*client);
		std::cout << "msg from " << address << ": " << msg << "\n";
	}
}


void send(const std::string& address, const std::string& cmd)
{
	s_sendmore (*client, address);
	s_sendmore (*client, "");
	s_send (*client, cmd);
}


int main () {
    zmq::context_t context(2);
    client = new zmq::socket_t(context, ZMQ_ROUTER);
    client->bind("tcp://0.0.0.0:123459");

	std::thread receiveThread(receiver, client);

	while (true)
	{
		std::string address;
		std::string cmd;

		std::cout << "Address: ";
		std::getline(std::cin, address);
		std::cout << "command: ";
		std::getline(std::cin, cmd);
		send(trim(address), trim(cmd));
    }
    return 0;
}


