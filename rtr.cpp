//
//  Custom routing Router to Mama (ROUTER to REQ)
//
// Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>

#include "zhelpers.hpp"

#define NBR_WORKERS 10

static void *
worker_thread (void *arg) {

    zmq::context_t * context = (zmq::context_t *)arg;
    zmq::socket_t worker (*context, ZMQ_REQ);
    
    //  We use a string identity for ease here
    s_set_id (worker);
    worker.connect("ipc://routing.ipc");

    int total = 0;
    while (1) {
        //  Tell the router we're ready for work
        s_send (worker, "ready");

        //  Get workload from router, until finished
        std::string workload = s_recv (worker);
        int finished = (workload.compare("END") == 0);
        
        if (finished) {
            std::cout << "Processed: " << total << " tasks" << std::endl;
            break;
        }
        total++;

        //  Do some random work
        s_sleep(within (100) + 1);
    }
    return (NULL);
}

int main () {
    zmq::context_t context(1);
    zmq::socket_t client (context, ZMQ_ROUTER);
    client.bind("ipc://routing.ipc");

    int worker_nbr;
    for (worker_nbr = 0; worker_nbr < NBR_WORKERS; worker_nbr++) {
        pthread_t worker;
        pthread_create (&worker, NULL, worker_thread, &context);
    }
    int task_nbr;
    for (task_nbr = 0; task_nbr < NBR_WORKERS * 10; task_nbr++) {
        //  LRU worker is next waiting in queue
        std::string address = s_recv (client);
        {
            // receiving and discarding'empty' message
            s_recv (client);
            // receiving and discarding 'ready' message
            s_recv (client);
        }

        s_sendmore (client, address);
        s_sendmore (client, "");
        s_send (client, "This is the workload");
    }
    //  Now ask mamas to shut down and report their results
    for (worker_nbr = 0; worker_nbr < NBR_WORKERS; worker_nbr++) {
        std::string address = s_recv (client);
        {
            // receiving and discarding'empty' message
            s_recv (client);
            // receiving and discarding 'ready' message
            s_recv (client);
        }

        s_sendmore (client, address);
        s_sendmore (client, "");
        s_send (client, "END");
    }
    sleep (1);              //  Give 0MQ/2.0.x time to flush output
    return 0;
}


