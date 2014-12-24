#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <signal.h>
#include <syslog.h>
#include <condition_variable>

extern std::condition_variable terminateSignaled;

void signal_handler(int sig)
{

	switch(sig)
	{
	case SIGHUP:
		syslog(LOG_WARNING, "Received SIGHUP signal.");
		break;
	case SIGTERM:
		syslog(LOG_WARNING, "Received SIGTERM signal.");
		terminateSignaled.notify_all();
		break;
	case SIGINT:
		syslog(LOG_WARNING, "Received SIGINT signal.");
		terminateSignaled.notify_all();
		break;
	default:
		syslog(LOG_WARNING, "Unhandled signal ");
		break;
	}
}

#endif


