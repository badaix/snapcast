//
//  Custom routing Router to Mama (ROUTER to REQ)
//
// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>

#include "zhelpers.hpp"

#define NBR_WORKERS 10


int main () {
    zmq::context_t context(1);
    zmq::socket_t worker (context, ZMQ_REQ);
    srand (time(NULL));
    //  We use a string identity for ease here
    s_set_id (worker);
    worker.connect("tcp://127.0.0.1:10000");

    int total = 0;
    while (1) {
        //  Tell the router we're ready for work
        s_send (worker, "ready");

        //  Get workload from router, until finished
        std::string workload = s_recv (worker);
std::cout << "workload: " << workload << "\n";
        int finished = (workload.compare("END") == 0);
        
        if (finished) {
            std::cout << "Processed: " << total << " tasks" << std::endl;
            break;
        }
        total++;

        //  Do some random work
        s_sleep(within (100) + 1);
    }
    return 0;
}


