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

#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <iterator>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iomanip>
#include <sys/sysinfo.h>
#include <sys/utsname.h>


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

// trim from start
static inline std::string ltrim_copy(const std::string &s)
{
	std::string str(s);
	return ltrim(str);
}

// trim from end
static inline std::string rtrim_copy(const std::string &s)
{
	std::string str(s);
	return rtrim(str);
}

// trim from both ends
static inline std::string trim_copy(const std::string &s)
{
	std::string str(s);
	return trim(str);
}

// decode %xx to char
static std::string uriDecode(const std::string& src) {
	std::string ret;
	char ch;
	for (size_t i=0; i<src.length(); i++)
	{
		if (int(src[i]) == 37)
		{
			unsigned int ii;
			sscanf(src.substr(i+1, 2).c_str(), "%x", &ii);
			ch = static_cast<char>(ii);
			ret += ch;
			i += 2;
		}
		else
		{
			ret += src[i];
		}
	}
	return (ret);
}



static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim))
	{
		elems.push_back(item);
	}
	return elems;
}


static std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}


static std::string execGetOutput(const std::string& cmd)
{
	std::shared_ptr<FILE> pipe(popen(cmd.c_str(), "r"), pclose);
	if (!pipe)
		return "";
	char buffer[1024];
	std::string result = "";
	while (!feof(pipe.get()))
	{
		if (fgets(buffer, 1024, pipe.get()) != NULL)
			result += buffer;
	}
	return trim(result);
}


#ifdef ANDROID
static std::string getProp(const std::string& prop)
{
	return execGetOutput("getprop " + prop);
}
#endif


static std::string getOS()
{
	std::string os;
#ifdef ANDROID
	os = trim_copy("Android " + getProp("ro.build.version.release"));
#else
	os = execGetOutput("lsb_release -d");
	if (os.find(":") != std::string::npos)
		os = trim_copy(os.substr(os.find(":") + 1));
#endif
	if (os.empty())
	{
		os = trim_copy(execGetOutput("cat /etc/os-release |grep PRETTY_NAME"));
		if (os.find("=") != std::string::npos)
		{
			os = trim_copy(os.substr(os.find("=") + 1));
			os.erase(std::remove(os.begin(), os.end(), '"'), os.end());
		}
	}
	if (os.empty())
	{
		utsname u;
		uname(&u);
		os = u.sysname;
	}
	return trim_copy(os);
}


static std::string getHostName()
{
#ifdef ANDROID
	std::string result = getProp("net.hostname");
	if (!result.empty())
		return result;
#endif
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	return hostname;
}


static std::string getArch()
{
	std::string arch;
#ifdef ANDROID
	arch = getProp("ro.product.cpu.abi");
	if (!arch.empty())
		return arch;
#endif
	arch = execGetOutput("arch");
	if (arch.empty())
		arch = execGetOutput("uname -i");
	if (arch.empty() || (arch == "unknown"))
		arch = execGetOutput("uname -m");
	return trim_copy(arch);
}


static long uptime()
{
	struct sysinfo info;
	sysinfo(&info);
	return info.uptime;
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

	for (; it != end; ++it)
	{
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
		{
			if (!(ifr.ifr_flags & IFF_LOOPBACK)) // don't count loopback
			{
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
				{
					success = 1;
					break;
				}
				else
				{
					std::stringstream ss;
					ss << "/sys/class/net/" << ifr.ifr_name << "/address";
					std::ifstream infile(ss.str().c_str());
					std::string line;
					if (infile.good() && std::getline(infile, line))
					{
						trim(line);
						if ((line.size() == 17) && (line[2] == ':'))
							return line;
					}
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


#endif


