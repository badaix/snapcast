/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"

#include <cctype>
#include <cerrno>
// #include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <locale>
#include <memory>
#ifndef WINDOWS
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#if !defined(WINDOWS) && !defined(FREEBSD)
#include <sys/sysinfo.h>
#endif
#ifdef MACOS
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOTypes.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#endif
#ifdef ANDROID
#include <sys/system_properties.h>
#endif
#ifdef WINDOWS
#include <chrono>
#include <direct.h>
#include <iphlpapi.h>
#include <versionhelpers.h>
#include <windows.h>
#include <winsock2.h>
#endif


namespace strutils = utils::string;


#ifndef WINDOWS
static std::string execGetOutput(const std::string& cmd)
{
    std::shared_ptr<::FILE> pipe(popen((cmd + " 2> /dev/null").c_str(), "r"),
                                 [](::FILE* stream)
                                 {
        if (stream != nullptr)
            pclose(stream);
    });
    if (!pipe)
        return "";
    char buffer[1024];
    std::string result;
    while (feof(pipe.get()) == 0)
    {
        if (fgets(buffer, 1024, pipe.get()) != nullptr)
            result += buffer;
    }
    return strutils::trim(result);
}
#endif


#ifdef ANDROID
static std::string getProp(const std::string& key, const std::string& def = "")
{
    std::string result(def);
    char cresult[PROP_VALUE_MAX + 1];
    if (__system_property_get(key.c_str(), cresult) > 0)
        result = cresult;
    return result;
}
#endif


static std::string getOS()
{
    static std::string os;

    if (!os.empty())
        return os;

#ifdef ANDROID
    os = strutils::trim_copy("Android " + getProp("ro.build.version.release"));
#elif WINDOWS
    if (/*IsWindows10OrGreater()*/ FALSE)
        os = "Windows 10";
    else if (IsWindows8Point1OrGreater())
        os = "Windows 8.1";
    else if (IsWindows8OrGreater())
        os = "Windows 8";
    else if (IsWindows7SP1OrGreater())
        os = "Windows 7 SP1";
    else if (IsWindows7OrGreater())
        os = "Windows 7";
    else if (IsWindowsVistaSP2OrGreater())
        os = "Windows Vista SP2";
    else if (IsWindowsVistaSP1OrGreater())
        os = "Windows Vista SP1";
    else if (IsWindowsVistaOrGreater())
        os = "Windows Vista";
    else if (IsWindowsXPSP3OrGreater())
        os = "Windows XP SP3";
    else if (IsWindowsXPSP2OrGreater())
        os = "Windows XP SP2";
    else if (IsWindowsXPSP1OrGreater())
        os = "Windows XP SP1";
    else if (IsWindowsXPOrGreater())
        os = "Windows XP";
    else
        os = "Unknown Windows";
#else
    os = execGetOutput("lsb_release -d");
    if ((os.find(':') != std::string::npos) && (os.find("lsb_release") == std::string::npos))
        os = strutils::trim_copy(os.substr(os.find(':') + 1));
#endif

#ifndef WINDOWS
    if (os.empty())
    {
        os = strutils::trim_copy(execGetOutput("grep /etc/os-release /etc/openwrt_release -e PRETTY_NAME -e DISTRIB_DESCRIPTION"));
        if (os.find('=') != std::string::npos)
        {
            os = strutils::trim_copy(os.substr(os.find('=') + 1));
            os.erase(std::remove(os.begin(), os.end(), '"'), os.end());
            os.erase(std::remove(os.begin(), os.end(), '\''), os.end());
        }
    }
    if (os.empty())
    {
        utsname u;
        uname(&u);
        os = u.sysname;
    }
#endif
    strutils::trim(os);
    return os;
}


static std::string getHostName()
{
#ifdef ANDROID
    std::string result = getProp("net.hostname");
    if (!result.empty())
        return result;
    result = getProp("ro.product.model");
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
#ifndef WINDOWS
    arch = execGetOutput("arch");
    if (arch.empty())
        arch = execGetOutput("uname -i");
    if (arch.empty() || (arch == "unknown"))
        arch = execGetOutput("uname -m");
#else
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    switch (sysInfo.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = "amd64";
            break;

        case PROCESSOR_ARCHITECTURE_ARM:
            arch = "arm";
            break;

        case PROCESSOR_ARCHITECTURE_IA64:
            arch = "ia64";
            break;

        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = "intel";
            break;

        default:
        case PROCESSOR_ARCHITECTURE_UNKNOWN:
            arch = "unknown";
            break;
    }
#endif
    return strutils::trim_copy(arch);
}

// Seems not to be used
// static std::chrono::seconds uptime()
// {
// #ifndef WINDOWS
// #ifndef FREEBSD
//     struct sysinfo info;
//     sysinfo(&info);
//     return std::chrono::seconds(info.uptime);
// #else
//     std::string uptime = execGetOutput("sysctl kern.boottime");
//     if ((uptime.find(" sec = ") != std::string::npos) && (uptime.find(",") != std::string::npos))
//     {
//         uptime = strutils::trim_copy(uptime.substr(uptime.find(" sec = ") + 7));
//         uptime.resize(uptime.find(","));
//         timeval now;
//         gettimeofday(&now, NULL);
//         try
//         {
//             return std::chrono::seconds(now.tv_sec - cpt::stoul(uptime));
//         }
//         catch (...)
//         {
//         }
//     }
//     return 0s;
// #endif
// #else
//     return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(GetTickCount()));
// #endif
// }


/// http://stackoverflow.com/questions/2174768/generating-random-uuids-in-linux
static std::string generateUUID()
{
    static bool initialized(false);
    if (!initialized)
    {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        initialized = true;
    }
    std::stringstream ss;
    ss << std::setfill('0') << std::hex << std::setw(4) << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff) << "-" << std::setw(4)
       << (std::rand() % 0xffff) << "-" << std::setw(4) << (std::rand() % 0xffff) << "-" << std::setw(4) << (std::rand() % 0xffff) << "-" << std::setw(4)
       << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff) << std::setw(4) << (std::rand() % 0xffff);
    return ss.str();
}


#ifndef WINDOWS
/// https://gist.github.com/OrangeTide/909204
static std::string getMacAddress(int sock)
{
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[16384];
    int success = 0;

    if (sock < 0)
        return "";

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) != 0)
        return "";

    struct ifreq* it = ifc.ifc_req;
    for (int i = 0; i < ifc.ifc_len;)
    {
/// some systems have ifr_addr.sa_len and adjust the length that way, but not mine. weird */
#ifdef FREEBSD
        size_t len = IFNAMSIZ + it->ifr_addr.sa_len;
#else
        size_t len = sizeof(*it);
#endif

        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
        {
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) // don't count loopback
            {
#ifdef MACOS
                /// Dirty Mac version
                struct ifaddrs *ifap, *ifaptr;
                unsigned char* ptr;

                if (getifaddrs(&ifap) == 0)
                {
                    for (ifaptr = ifap; ifaptr != NULL; ifaptr = ifaptr->ifa_next)
                    {
                        //						std::cout << ifaptr->ifa_name << ", " << ifreq->ifr_name << "\n";
                        if (strcmp(ifaptr->ifa_name, it->ifr_name) != 0)
                            continue;
                        if (ifaptr->ifa_addr->sa_family == AF_LINK)
                        {
                            ptr = (unsigned char*)LLADDR((struct sockaddr_dl*)ifaptr->ifa_addr);
                            char mac[19];
                            sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3), *(ptr + 4), *(ptr + 5));
                            if (strcmp(mac, "00:00:00:00:00:00") == 0)
                                continue;
                            freeifaddrs(ifap);
                            return mac;
                        }
                    }
                    freeifaddrs(ifap);
                }
#endif

#ifdef FREEBSD
                if (ioctl(sock, SIOCGIFMAC, &ifr) == 0)
#else
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
#endif
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
                        strutils::trim(line);
                        if ((line.size() == 17) && (line[2] == ':'))
                            return line;
                    }
                }
            }
        }
        else
        { /* handle error */
        }

        it = (struct ifreq*)((char*)it + len);
        i += len;
    }

    if (!success)
        return "";

    char mac[19];
#ifndef FREEBSD
    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", (unsigned char)ifr.ifr_hwaddr.sa_data[0], (unsigned char)ifr.ifr_hwaddr.sa_data[1],
            (unsigned char)ifr.ifr_hwaddr.sa_data[2], (unsigned char)ifr.ifr_hwaddr.sa_data[3], (unsigned char)ifr.ifr_hwaddr.sa_data[4],
            (unsigned char)ifr.ifr_hwaddr.sa_data[5]);
#else
    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[0], (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[1],
            (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[2], (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[3],
            (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[4], (unsigned char)ifr.ifr_ifru.ifru_addr.sa_data[5]);
#endif
    return mac;
}
#else
static std::string getMacAddress(const std::string& address)
{
    IP_ADAPTER_INFO* first;
    IP_ADAPTER_INFO* pos;
    ULONG bufferLength = sizeof(IP_ADAPTER_INFO);
    first = (IP_ADAPTER_INFO*)malloc(bufferLength);

    if (GetAdaptersInfo(first, &bufferLength) == ERROR_BUFFER_OVERFLOW)
    {
        free(first);
        first = (IP_ADAPTER_INFO*)malloc(bufferLength);
    }

    char mac[19];
    if (GetAdaptersInfo(first, &bufferLength) == NO_ERROR)
        for (pos = first; pos != NULL; pos = pos->Next)
        {
            IP_ADDR_STRING* firstAddr = &pos->IpAddressList;
            IP_ADDR_STRING* posAddr;
            for (posAddr = firstAddr; posAddr != NULL; posAddr = posAddr->Next)
                if (_stricmp(posAddr->IpAddress.String, address.c_str()) == 0)
                {
                    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", pos->Address[0], pos->Address[1], pos->Address[2], pos->Address[3], pos->Address[4],
                            pos->Address[5]);

                    free(first);
                    return mac;
                }
        }
    else
        free(first);

    return mac;
}
#endif

static std::string getHostId(const std::string& defaultId = "")
{
    std::string result = strutils::trim_copy(defaultId);

    if (!result.empty()                    // default provided
        && (result != "00:00:00:00:00:00") // default mac returned by getMaxAddress if it fails
        && (result != "02:00:00:00:00:00") // the Android API will return "02:00:00:00:00:00" for WifiInfo.getMacAddress()
        && (result != "ac:de:48:00:11:22") // iBridge interface on new MacBook Pro (later 2016)
    )
        return result;

#ifdef MACOS
    /// https://stackoverflow.com/questions/933460/unique-hardware-id-in-mac-os-x
    /// About this Mac, Hardware-UUID
    char buf[64];
    io_registry_entry_t ioRegistryRoot = IORegistryEntryFromPath(kIOMasterPortDefault, "IOService:/");
    CFStringRef uuidCf = (CFStringRef)IORegistryEntryCreateCFProperty(ioRegistryRoot, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
    IOObjectRelease(ioRegistryRoot);
    if (CFStringGetCString(uuidCf, buf, 64, kCFStringEncodingMacRoman))
        result = buf;
    CFRelease(uuidCf);
#elif ANDROID
    result = getProp("ro.serialno");
#endif

    //#else
    //	// on embedded platforms it's
    //  // - either not there
    //  // - or not unique, or changes during boot
    //  // - or changes during boot
    //	std::ifstream infile("/var/lib/dbus/machine-id");
    //	if (infile.good())
    //		std::getline(infile, result);
    //#endif
    strutils::trim(result);
    if (!result.empty())
        return result;

    /// The host name should be unique enough in a LAN
    return getHostName();
}


#endif
