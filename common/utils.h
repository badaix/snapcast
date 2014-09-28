#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <sys/ioctl.h>
#include <net/if.h> 
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iomanip>

// trim from start
static inline std::string &ltrim(std::string &s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

// trim from end
static inline std::string &rtrim(std::string &s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

// trim from both ends
static inline std::string &trim(std::string &s)
{
	return ltrim(rtrim(s));
}


static std::string getMacAddress(int sock)
{
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    if (sock < 0)
		return "";

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) 
		return "";

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
                    break;
                }
            }
        }
        else { /* handle error */ }
    }

    if (!success)
		return "";

	char mac[19];
    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", 
        (unsigned char)ifr.ifr_hwaddr.sa_data[0], (unsigned char)ifr.ifr_hwaddr.sa_data[1], (unsigned char)ifr.ifr_hwaddr.sa_data[2],
        (unsigned char)ifr.ifr_hwaddr.sa_data[3], (unsigned char)ifr.ifr_hwaddr.sa_data[4], (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
	return mac;
}
/*	std::ifstream t("/sys/class/net/eth0/address");
	std::string str((std::istreambuf_iterator<char>(t)),
	                std::istreambuf_iterator<char>());
	return trim(str);
}
*/


static void daemonize()
{
	/* Our process ID and Session ID */
	pid_t pid, sid;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);

	/* If we got a good PID, then
	   we can exit the parent process. */
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* Change the file mode mask */
	umask(0);

	/* Open any logs here */

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0)
	{
		/* Log the failure */
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory */
	if ((chdir("/")) < 0)
	{
		/* Log the failure */
		exit(EXIT_FAILURE);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}




#endif


