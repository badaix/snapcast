//
//  Custom routing Router to Mama (ROUTER to REQ)
//
// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>

#include "zhelpers.hpp"

#define NBR_WORKERS 10


int main () {
    zmq::context_t context(2);
    zmq::socket_t client (context, ZMQ_ROUTER);
    client.bind("tcp://127.0.0.1:10000");

    int task_nbr;
	while (true)
	{
        //  LRU worker is next waiting in queue
        std::string address = s_recv (client);
		std::cout << "Address: " << address << "\n";
		
        // receiving and discarding'empty' message
        s_recv (client);
        // receiving and discarding 'ready' message
        std::string msg = s_recv (client);
		std::cout << "msg: " << msg << "\n";

        s_sendmore (client, address);
        s_sendmore (client, "");
        s_send (client, std::string("hello: ") + address);
    }
    return 0;
}


