/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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

#pragma once


// local headers
#include "common/json.hpp"
#include "common/utils.hpp"
#include "common/utils/string_utils.hpp"

// standard headers
#include <memory>
#include <mutex>
#include <string>
#include <sys/time.h>
#include <vector>


namespace strutils = utils::string;
using json = nlohmann::json;

struct ClientInfo;
struct Group;

using ClientInfoPtr = std::shared_ptr<ClientInfo>;
using GroupPtr = std::shared_ptr<Group>;


template <typename T>
T jGet(const json& j, const std::string& what, const T& def)
{
    try
    {
        if (!j.count(what))
            return def;
        return j[what].get<T>();
    }
    catch (...)
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
    Host() : name(""), mac(""), os(""), arch(""), ip("")
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
        name = strutils::trim_copy(jGet<std::string>(j, "name", ""));
        mac = strutils::trim_copy(jGet<std::string>(j, "mac", ""));
        os = strutils::trim_copy(jGet<std::string>(j, "os", ""));
        arch = strutils::trim_copy(jGet<std::string>(j, "arch", ""));
        ip = strutils::trim_copy(jGet<std::string>(j, "ip", ""));
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
    ClientConfig() : name(""), volume(100), latency(0), instance(1)
    {
    }

    void fromJson(const json& j)
    {
        name = strutils::trim_copy(jGet<std::string>(j, "name", ""));
        volume.fromJson(j["volume"]);
        latency = jGet<int32_t>(j, "latency", 0);
        instance = jGet<size_t>(j, "instance", 1);
    }

    json toJson()
    {
        json j;
        j["name"] = strutils::trim_copy(name);
        j["volume"] = volume.toJson();
        j["latency"] = latency;
        j["instance"] = instance;
        return j;
    }

    std::string name;
    Volume volume;
    int32_t latency;
    size_t instance;
};



struct Snapcast
{
    Snapcast(const std::string& _name = "", const std::string& _version = "") : name(_name), version(_version), protocolVersion(1)
    {
    }

    virtual ~Snapcast() = default;

    virtual void fromJson(const json& j)
    {
        name = strutils::trim_copy(jGet<std::string>(j, "name", ""));
        version = strutils::trim_copy(jGet<std::string>(j, "version", ""));
        protocolVersion = jGet<int>(j, "protocolVersion", 1);
    }

    virtual json toJson()
    {
        json j;
        j["name"] = strutils::trim_copy(name);
        j["version"] = strutils::trim_copy(version);
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

    void fromJson(const json& j) override
    {
        Snapcast::fromJson(j);
        controlProtocolVersion = jGet<int>(j, "controlProtocolVersion", 1);
    }

    json toJson() override
    {
        json j = Snapcast::toJson();
        j["controlProtocolVersion"] = controlProtocolVersion;
        return j;
    }

    int controlProtocolVersion;
};


struct ClientInfo
{
    ClientInfo(const std::string& _clientId = "") : id(_clientId), connected(false)
    {
        lastSeen.tv_sec = 0;
        lastSeen.tv_usec = 0;
    }

    void fromJson(const json& j)
    {
        host.fromJson(j["host"]);
        id = jGet<std::string>(j, "id", host.mac);
        snapclient.fromJson(j["snapclient"]);
        config.fromJson(j["config"]);
        lastSeen.tv_sec = jGet<int32_t>(j["lastSeen"], "sec", 0);
        lastSeen.tv_usec = jGet<int32_t>(j["lastSeen"], "usec", 0);
        connected = jGet<bool>(j, "connected", true);
        if (j.contains("systemInfo"))
        {
            systemInfo = j["systemInfo"].template get<json>();
        }
    }

    json toJson()
    {
        json j;
        j["id"] = id;
        j["host"] = host.toJson();
        j["snapclient"] = snapclient.toJson();
        j["config"] = config.toJson();
        j["lastSeen"]["sec"] = lastSeen.tv_sec;
        j["lastSeen"]["usec"] = lastSeen.tv_usec;
        j["connected"] = connected;
        j["systemInfo"] = systemInfo;
        return j;
    }

    std::string id;
    Host host;
    Snapclient snapclient;
    ClientConfig config;
    timeval lastSeen;
    bool connected;
    json systemInfo;
};


struct Group
{
    Group(const ClientInfoPtr client = nullptr) : muted(false)
    {
        if (client)
            id = client->id;
        id = generateUUID();
    }

    void fromJson(const json& j)
    {
        name = strutils::trim_copy(jGet<std::string>(j, "name", ""));
        id = strutils::trim_copy(jGet<std::string>(j, "id", ""));
        streamId = strutils::trim_copy(jGet<std::string>(j, "stream_id", ""));
        muted = jGet<bool>(j, "muted", false);
        clients.clear();
        if (j.count("clients"))
        {
            for (const auto& jClient : j["clients"])
            {
                ClientInfoPtr client = std::make_shared<ClientInfo>();
                client->fromJson(jClient);
                client->connected = false;
                addClient(client);
            }
        }
    }

    json toJson()
    {
        json j;
        j["name"] = strutils::trim_copy(name);
        j["id"] = strutils::trim_copy(id);
        j["stream_id"] = strutils::trim_copy(streamId);
        j["muted"] = muted;

        json jClients = json::array();
        for (const auto& client : clients)
            jClients.push_back(client->toJson());
        j["clients"] = jClients;
        return j;
    }

    ClientInfoPtr removeClient(const std::string& clientId)
    {
        for (auto iter = clients.begin(); iter != clients.end(); ++iter)
        {
            if ((*iter)->id == clientId)
            {
                clients.erase(iter);
                return (*iter);
            }
        }
        return nullptr;
    }

    ClientInfoPtr removeClient(ClientInfoPtr client)
    {
        if (!client)
            return nullptr;

        return removeClient(client->id);
    }

    ClientInfoPtr getClient(const std::string& clientId)
    {
        for (const auto& client : clients)
        {
            if (client->id == clientId)
                return client;
        }
        return nullptr;
    }

    void addClient(ClientInfoPtr client)
    {
        if (!client)
            return;

        for (const auto& c : clients)
        {
            if (c->id == client->id)
                return;
        }

        clients.push_back(client);
        /*		sort(clients.begin(), clients.end(),
                                [](const ClientInfoPtr a, const ClientInfoPtr b) -> bool
                                {
                                        return a.name > b.name;
                                });
        */
    }

    bool empty() const
    {
        return clients.empty();
    }

    std::string name;
    std::string id;
    std::string streamId;
    bool muted;
    std::vector<ClientInfoPtr> clients;
};


class Config
{
public:
    static Config& instance()
    {
        static Config instance_;
        return instance_;
    }

    ClientInfoPtr getClientInfo(const std::string& clientId) const;
    GroupPtr addClientInfo(const std::string& clientId);
    GroupPtr addClientInfo(ClientInfoPtr client);
    void remove(ClientInfoPtr client);
    void remove(GroupPtr group, bool force = false);

    //	GroupPtr removeFromGroup(const std::string& groupId, const std::string& clientId);
    //	GroupPtr setGroupForClient(const std::string& groupId, const std::string& clientId);

    GroupPtr getGroupFromClient(const std::string& clientId);
    GroupPtr getGroupFromClient(ClientInfoPtr client);
    GroupPtr getGroup(const std::string& groupId) const;

    json getGroups() const;
    json getServerStatus(const json& streams) const;

    void save();

    void init(const std::string& root_directory = "", const std::string& user = "", const std::string& group = "");

    std::vector<GroupPtr> groups;

    std::mutex& getMutex();

private:
    Config() = default;
    ~Config();
    mutable std::recursive_mutex mutex_;
    std::mutex client_mutex_;
    std::string filename_;
};
