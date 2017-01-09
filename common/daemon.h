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

#ifndef DAEMONIZE_H
#define DAEMONIZE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include "common/snapException.h"
#include "common/strCompat.h"
#include "common/utils.h"


int pidFilehandle;

void daemonize(const std::string& user, const std::string& group, const std::string& pidfile)
{
	if (pidfile.empty() || pidfile.find('/') == std::string::npos)
		throw SnapException("invalid pid file \"" + pidfile + "\"");

	std::string pidfileDir(pidfile.substr(0, pidfile.find_last_of('/')));
	mkdirRecursive(pidfileDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	/// Ensure only one copy
	pidFilehandle = open(pidfile.c_str(), O_RDWR|O_CREAT, 0644);
	if (pidFilehandle == -1 )
	{
		/// Couldn't open lock file
		throw SnapException("Could not open PID lock file \"" + pidfile + "\"");
	}

	uid_t user_uid = (uid_t)-1;
	gid_t user_gid = (gid_t)-1;
	std::string user_name;
#ifdef FREEBSD
	bool had_group = false;
#endif

	if (!user.empty())
	{
		struct passwd *pwd = getpwnam(user.c_str());
		if (pwd == nullptr)
			throw SnapException("no such user \"" + user + "\"");
		user_uid = pwd->pw_uid;
		user_gid = pwd->pw_gid;
		user_name = strdup(user.c_str());
		/// this is needed by libs such as arts
		setenv("HOME", pwd->pw_dir, true);
	}

	if (!group.empty()) {
		struct group *grp = getgrnam(group.c_str());
		if (grp == nullptr)
			throw SnapException("no such group \"" + group + "\"");
		user_gid = grp->gr_gid;
#ifdef FREEBSD
		had_group = true;
#endif
	}

	/// set gid
	if (user_gid != (gid_t)-1 && user_gid != getgid() && setgid(user_gid) == -1)
		throw SnapException("Failed to set group " + cpt::to_string((int)user_gid));

//#if defined(FREEBSD) && !defined(MACOS)
//#ifdef FREEBSD
	/// init supplementary groups
	/// (must be done before we change our uid)
	/// no need to set the new user's supplementary groups if we are already this user
//	if (!had_group && user_uid != getuid() && initgroups(user_name, user_gid) == -1)
//		throw SnapException("Failed to set supplementary groups of user \"" + user + "\"");
//#endif
	/// set uid
	if (user_uid != (uid_t)-1 && user_uid != getuid() && setuid(user_uid) == -1)
		throw SnapException("Failed to set user " + user);

	/// Our process ID and Session ID
	pid_t pid, sid;

	/// Fork off the parent process
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);

	/// If we got a good PID, then we can exit the parent process.
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/// Change the file mode mask
	umask(0);

	/// Open any logs here

	/// Create a new SID for the child process
	sid = setsid();
	if (sid < 0)
	{
		/// Log the failure
		exit(EXIT_FAILURE);
	}

	/// Change the current working directory
	if ((chdir("/")) < 0)
	{
		/// Log the failure
		exit(EXIT_FAILURE);
	}

	/// Try to lock file
	if (lockf(pidFilehandle, F_TLOCK, 0) == -1)
		throw SnapException("Could not lock PID lock file \"" + pidfile + "\"");

	char str[10];
	/// Get and format PID
	sprintf(str, "%d\n", getpid());

	/// write pid to lockfile
	if (write(pidFilehandle, str, strlen(str)) != (int)strlen(str))
		throw SnapException("Could not write PID to lock file \"" + pidfile + "\"");

	/// Close out the standard file descriptors
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}


void daemonShutdown()
{
	close(pidFilehandle);
}


#endif


