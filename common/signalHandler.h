#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <signal.h>
#include <syslog.h>

extern bool g_terminated;

void signal_handler(int sig)
{

    switch(sig)
    {
    case SIGHUP:
        syslog(LOG_WARNING, "Received SIGHUP signal.");
        break;
    case SIGTERM:
        syslog(LOG_WARNING, "Received SIGTERM signal.");
        g_terminated = true;
        break;
    default:
        syslog(LOG_WARNING, "Unhandled signal ");
        break;
    }
}

#endif


