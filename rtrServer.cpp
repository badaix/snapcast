//
//  Custom routing Router to Mama (ROUTER to REQ)
//
// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>

#include <thread> 
#include <istream>
#include "zhelpers.hpp"



void receiver(zmq::socket_t* client)
{
	while (true)
	{
		std::string address = s_recv (*client);
		std::cout << "Address: " << address << "\n";
	    // receiving and discarding'empty' message
	    s_recv (*client);
	    // receiving and discarding 'ready' message
	    std::string msg = s_recv (*client);
		std::cout << "msg: " << msg << "\n";
	}
}



int main () {
    zmq::context_t context(2);
    zmq::socket_t client (context, ZMQ_ROUTER);
    client.bind("tcp://0.0.0.0:123459");

	std::thread receiveThread(receiver, &client);

	while (true)
	{
		std::string address;
		std::string cmd;

		std::getline(std::cin, address);
		std::getline(std::cin, cmd);
        s_sendmore (client, address);
        s_sendmore (client, "");
        s_send (client, cmd);
    }
    return 0;
}


