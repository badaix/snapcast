//
//  Custom routing Router to Mama (ROUTER to REQ)
//
// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>


#include <thread> 
#include <vector>
#include <string>
#include <fstream>
#include <iterator>
#include "zhelpers.hpp"
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h> 
#include "utils.h"

using namespace std;



void alive(zmq::socket_t* worker)
{
    //  Tell the router we're ready for work
    while (1) {
	    s_send (*worker, "ping");
		sleep(1);
    }
}


void control(zmq::socket_t* worker)
{
    //  Tell the router we're ready for work
//    s_send (*worker, "ready");
    while (1) {
        std::string cmd = s_recv (*worker);
		vector<std::string> splitCmd = split(cmd);
		for (size_t n=0; n<splitCmd.size(); ++n)
			std::cout << "cmd: " << splitCmd[n] << "\n";
		s_send(*worker, "ACK " + cmd);
    }
}


int main () {
	cout << getMacAddress() << "\n";
    zmq::context_t context(1);
    zmq::socket_t worker (context, ZMQ_DEALER);
    srand (time(NULL));
    //  We use a string identity for ease here
	string macAddress = getMacAddress();
    worker.setsockopt(ZMQ_IDENTITY, macAddress.c_str(), macAddress.length());
    worker.connect("tcp://127.0.0.1:123459");

	std::thread controlThread(control, &worker);
	std::thread aliveThread(alive, &worker);
	controlThread.join();
    return 0;
}




