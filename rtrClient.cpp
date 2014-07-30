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

using namespace std;


std::string getMacAddress()
{
	std::ifstream t("/sys/class/net/eth0/address");
	std::string str((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());
	str.erase(std::find_if(str.rbegin(), str.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), str.end());
	return str;
}



void control()
{
    zmq::context_t context(1);
    zmq::socket_t worker (context, ZMQ_REQ);
    srand (time(NULL));
    //  We use a string identity for ease here
	string macAddress = getMacAddress();
    worker.setsockopt(ZMQ_IDENTITY, macAddress.c_str(), macAddress.length());
    worker.connect("tcp://127.0.0.1:10000");

    //  Tell the router we're ready for work
    s_send (worker, "ready");
    while (1) {
        std::string cmd = s_recv (worker);
		istringstream iss(cmd);
		vector<std::string> splitCmd;
		copy(istream_iterator<string>(iss), istream_iterator<string>(), back_inserter<vector<string> >(splitCmd));
		for (size_t n=0; n<splitCmd.size(); ++n)
			std::cout << "cmd: " << splitCmd[n] << "\n";
		s_send(worker, "ACK " + cmd);
    }
}


int main () {
	cout << getMacAddress() << "\n";
	std::thread controlThread(control);
	controlThread.join();
    return 0;
}




