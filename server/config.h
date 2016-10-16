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

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <memory>
#include <vector>
#include <sys/time.h>
#include "externals/json.hpp"
#include "common/utils.h"


using json = nlohmann::json;

template<typename T>
T jGet(const json& j, const std::string& what, const T& def)
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


struct Volume
{
	Volume(uint16_t _percent = 100, bool _muted = false) : percent(_percent), muted(_muted)
	{
	}

	void fromJson(const json& j)
	{
		percent = jGet<uint16_t>(j, "percent", percent);
		muted = jGet<bool>(j, "muted", muted);
	}

	json toJson()
	{
		json j;
		j["percent"] = percent;
		j["muted"] = muted;
		return j;
	}

	uint16_t percent;
	bool muted;
};



struct Host
{
	Host(const std::string& _macAddress = "") : name(""), mac(_macAddress), os(""), arch(""), ip("")
	{
	}

	void update() 
	{
		name = getHostName();
		os = getOS();
		arch = getArch();
	}

	void fromJson(const json& j)
	{
		name = trim_copy(jGet<std::string>(j, "name", ""));
		mac = trim_copy(jGet<std::string>(j, "mac", ""));
		os = trim_copy(jGet<std::string>(j, "os", ""));
		arch = trim_copy(jGet<std::string>(j, "arch", ""));
		ip = trim_copy(jGet<std::string>(j, "ip", ""));
	}

	json toJson()
	{
		json j;
		j["name"] = name;
		j["mac"] = mac;
		j["os"] = os;
		j["arch"] = arch;
		j["ip"] = ip;
		return j;
	}

	std::string name;
	std::string mac;
	std::string os;
	std::string arch;
	std::string ip;
};


struct ClientConfig
{
	ClientConfig() : name(""), volume(100), latency(0), streamId("")
	{
	}

	void fromJson(const json& j)
	{
		name = trim_copy(jGet<std::string>(j, "name", ""));
		volume.fromJson(j["volume"]);
		latency = jGet<int32_t>(j, "latency", 0);
		streamId = trim_copy(jGet<std::string>(j, "stream", ""));
	}

	json toJson()
	{
		json j;
		j["name"] = trim_copy(name);
		j["volume"] = volume.toJson();
		j["latency"] = latency;
		j["stream"] = trim_copy(streamId);
		return j;
	}

	std::string name;
	Volume volume;
	int32_t latency;
	std::string streamId;
};



struct Snapcast
{
	Snapcast(const std::string& _name = "", const std::string& _version = "") : name(_name), version(_version), protocolVersion(1)
	{
	}

	virtual ~Snapcast()
	{
	}

	virtual void fromJson(const json& j)
	{
		name = trim_copy(jGet<std::string>(j, "name", ""));
		version = trim_copy(jGet<std::string>(j, "version", ""));
		protocolVersion = jGet<int>(j, "protocolVersion", 1);
	}

	virtual json toJson()
	{
		json j;
		j["name"] = trim_copy(name);
		j["version"] = trim_copy(version);
		j["protocolVersion"] = protocolVersion;
		return j;
	}

	std::string name;
	std::string version;
	int protocolVersion;
};


struct Snapclient : public Snapcast
{
	Snapclient(const std::string& _name = "", const std::string& _version = "") : Snapcast(_name, _version)
	{
	}
};


struct Snapserver : public Snapcast
{
	Snapserver(const std::string& _name = "", const std::string& _version = "") : Snapcast(_name, _version), controlProtocolVersion(1)
	{
	}

	virtual void fromJson(const json& j)
	{
		Snapcast::fromJson(j);
		controlProtocolVersion = jGet<int>(j, "controlProtocolVersion", 1);
	}

	virtual json toJson()
	{
		json j = Snapcast::toJson();
		j["controlProtocolVersion"] = controlProtocolVersion;
		return j;
	}

	int controlProtocolVersion;
};


struct ClientInfo
{
	ClientInfo(const std::string& _macAddress = "") : host(_macAddress), connected(false)
	{
		lastSeen.tv_sec = 0;
		lastSeen.tv_usec = 0;
	}

	void fromJson(const json& j)
	{
		if (j.count("host") && !j["host"].is_string())
		{
			host.fromJson(j["host"]);
		}
		else
		{
			host.ip = jGet<std::string>(j, "IP", "");
			host.mac = jGet<std::string>(j, "MAC", "");
			host.name = jGet<std::string>(j, "host", "");
		}

		if (j.count("snapclient"))
			snapclient.fromJson(j["snapclient"]);
		else
			snapclient.version = jGet<std::string>(j, "version", "");

		if (j.count("config"))
		{
			config.fromJson(j["config"]);
		}
		else
		{
			config.name = trim_copy(jGet<std::string>(j, "name", ""));
			config.volume.fromJson(j["volume"]);
			config.latency = jGet<int32_t>(j, "latency", 0);
			config.streamId = trim_copy(jGet<std::string>(j, "stream", ""));
		}

		lastSeen.tv_sec = jGet<int32_t>(j["lastSeen"], "sec", 0);
		lastSeen.tv_usec = jGet<int32_t>(j["lastSeen"], "usec", 0);
		connected = jGet<bool>(j, "connected", true);
	}

	json toJson()
	{
		json j;
		j["host"] = host.toJson();
		j["snapclient"] = snapclient.toJson();
		j["config"] = config.toJson();
		j["lastSeen"]["sec"] = lastSeen.tv_sec;
		j["lastSeen"]["usec"] = lastSeen.tv_usec;
		j["connected"] = connected;
		return j;
	}

	Host host;
	Snapclient snapclient;
	ClientConfig config;
	timeval lastSeen;
	bool connected;
};

typedef std::shared_ptr<ClientInfo> ClientInfoPtr;


class Config
{
public:
	static Config& instance()
	{
		static Config instance_;
		return instance_;
	}

	ClientInfoPtr getClientInfo(const std::string& mac, bool add = true);
	void remove(ClientInfoPtr client);

	std::vector<ClientInfoPtr> clients;
	json getClientInfos() const;

	void save();

private:
	Config();
	std::string filename_;
};


#endif
