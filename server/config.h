/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <sys/time.h>
#include "json.hpp"

using json = nlohmann::json;

template<typename T>
T jGet(json j, const std::string& what, const T& def)
{
	try
	{
		return j[what].get<T>();
	}
	catch(...)
	{
		return def;
	}
}


struct ClientInfo
{
	void fromJson(const json& j)
	{
		macAddress = jGet<std::string>(j, "MAC", "");
		ipAddress = jGet<std::string>(j, "IP", "");
		hostName = jGet<std::string>(j, "host", "");
		version = jGet<std::string>(j, "version", "");
		name = jGet<std::string>(j, "name", "");
		volume = jGet<double>(j, "volume", 1.0);
		lastSeen.tv_sec = jGet<int32_t>(j["lastSeen"], "sec", 0);
		lastSeen.tv_usec = jGet<int32_t>(j["lastSeen"], "usec", 0);
		connected = jGet<bool>(j, "connected", true);
	}

	json toJson()
	{
		json j;
		j["MAC"] = macAddress;
		j["IP"] = ipAddress;
		j["host"] = hostName;
		j["version"] = version;
		j["name"] = name;
		j["volume"] = volume;
		j["lastSeen"]["sec"] = lastSeen.tv_sec;
		j["lastSeen"]["usec"] = lastSeen.tv_usec;
		j["connected"] = connected;
		return j;
	}

	std::string macAddress;
	std::string ipAddress;
	std::string hostName;
	std::string version;
	std::string name;
	double volume;
	timeval lastSeen;
	bool connected;
};


class Config
{
public:
	static Config& instance()
	{
		static Config instance_;
		return instance_;
	}

	void test();

private:
	Config();


};


#endif
