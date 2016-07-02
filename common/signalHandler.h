/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <signal.h>
#include <syslog.h>

extern volatile sig_atomic_t g_terminated;

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
	case SIGINT:
		syslog(LOG_WARNING, "Received SIGINT signal.");
		g_terminated = true;
		break;
	default:
		syslog(LOG_WARNING, "Unhandled signal ");
		break;
	}
}

#endif


