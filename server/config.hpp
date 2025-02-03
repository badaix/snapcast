/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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


/// Config item base class
struct JsonConfigItem
{
    /// Read config item from json object @p j
    virtual void fromJson(const json& j) = 0;
    /// @return config item serialized to json
    virtual json toJson() = 0;

    /// d'tor
    virtual ~JsonConfigItem() = default;

protected:
    /// @return value for key @p what or @p def, if not found. Result is casted to T.
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
};

/// Volume config
struct Volume : public JsonConfigItem
{
    /// c'tor
    explicit Volume(uint16_t _percent = 100, bool _muted = false) : percent(_percent), muted(_muted)
    {
    }

    void fromJson(const json& j) override
    {
        percent = jGet<uint16_t>(j, "percent", percent);
        muted = jGet<bool>(j, "muted", muted);
    }

    json toJson() override
    {
        json j;
        j["percent"] = percent;
        j["muted"] = muted;
        return j;
    }

    uint16_t percent; ///< volume in percent
    bool muted;       ///< muted
};


/// Host config
struct Host : public JsonConfigItem
{
    /// c'tor
    Host() = default;

    /// Update name, os and arch to actual values
    void update()
    {
        name = getHostName();
        os = getOS();
        arch = getArch();
    }

    void fromJson(const json& j) override
    {
        name = strutils::trim_copy(jGet<std::string>(j, "name", ""));
        mac = strutils::trim_copy(jGet<std::string>(j, "mac", ""));
        os = strutils::trim_copy(jGet<std::string>(j, "os", ""));
        arch = strutils::trim_copy(jGet<std::string>(j, "arch", ""));
        ip = strutils::trim_copy(jGet<std::string>(j, "ip", ""));
    }

    json toJson() override
    {
        json j;
        j["name"] = name;
        j["mac"] = mac;
        j["os"] = os;
        j["arch"] = arch;
        j["ip"] = ip;
        return j;
    }

    std::string name; ///< host name
    std::string mac;  ///< mac address
    std::string os;   /// OS
    std::string arch; /// CPU architecture
    std::string ip;   /// IP address
};


/// Client config
struct ClientConfig : public JsonConfigItem
{
    ClientConfig() : volume(100)
    {
    }

    void fromJson(const json& j) override
    {
        name = strutils::trim_copy(jGet<std::string>(j, "name", ""));
        volume.fromJson(j["volume"]);
        latency = jGet<int32_t>(j, "latency", 0);
        instance = jGet<size_t>(j, "instance", 1);
    }

    json toJson() override
    {
        json j;
        j["name"] = strutils::trim_copy(name);
        j["volume"] = volume.toJson();
        j["latency"] = latency;
        j["instance"] = instance;
        return j;
    }

    std::string name;   ///< client name
    Volume volume;      ///< client volume
    int32_t latency{0}; ///< additional latency
    size_t instance{1}; ///< instance id
};


/// Snapcast base config
struct Snapcast : public JsonConfigItem
{
    /// c'tor
    explicit Snapcast(std::string _name = "", std::string _version = "") : name(std::move(_name)), version(std::move(_version))
    {
    }

    virtual ~Snapcast() = default;

    void fromJson(const json& j) override
    {
        name = strutils::trim_copy(jGet<std::string>(j, "name", ""));
        version = strutils::trim_copy(jGet<std::string>(j, "version", ""));
        protocolVersion = jGet<int>(j, "protocolVersion", 1);
    }

    json toJson() override
    {
        json j;
        j["name"] = strutils::trim_copy(name);
        j["version"] = strutils::trim_copy(version);
        j["protocolVersion"] = protocolVersion;
        return j;
    }

    std::string name;       ///< name of the client or server
    std::string version;    ///< software version
    int protocolVersion{1}; ///< binary protocol version
};


/// Snapclient config
struct Snapclient : public Snapcast
{
    explicit Snapclient(std::string _name = "", std::string _version = "") : Snapcast(std::move(_name), std::move(_version))
    {
    }
};


/// Snapserver config
struct Snapserver : public Snapcast
{
    /// c'tor
    explicit Snapserver(std::string _name = "", std::string _version = "") : Snapcast(std::move(_name), std::move(_version))
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

    int controlProtocolVersion{1}; ///< control protocol version
};


/// Client config
struct ClientInfo : public JsonConfigItem
{
    explicit ClientInfo(std::string _clientId = "") : id(std::move(_clientId))
    {
        lastSeen.tv_sec = 0;
        lastSeen.tv_usec = 0;
    }

    void fromJson(const json& j) override
    {
        host.fromJson(j["host"]);
        id = jGet<std::string>(j, "id", host.mac);
        snapclient.fromJson(j["snapclient"]);
        config.fromJson(j["config"]);
        lastSeen.tv_sec = jGet<int32_t>(j["lastSeen"], "sec", 0);
        lastSeen.tv_usec = jGet<int32_t>(j["lastSeen"], "usec", 0);
        connected = jGet<bool>(j, "connected", true);
    }

    json toJson() override
    {
        json j;
        j["id"] = id;
        j["host"] = host.toJson();
        j["snapclient"] = snapclient.toJson();
        j["config"] = config.toJson();
        j["lastSeen"]["sec"] = lastSeen.tv_sec;
        j["lastSeen"]["usec"] = lastSeen.tv_usec;
        j["connected"] = connected;
        return j;
    }

    std::string id;        ///< unique client id
    Host host;             ///< host
    Snapclient snapclient; ///< Snapclient
    ClientConfig config;   ///< Client config
    timeval lastSeen;      ///< last online
    bool connected{false}; ///< connected to server?
};


/// Group config
struct Group : public JsonConfigItem
{
    /// c'tor
    explicit Group(const ClientInfoPtr& client = nullptr)
    {
        if (client)
            id = client->id;
        id = generateUUID();
    }

    void fromJson(const json& j) override
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

    json toJson() override
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

    /// Remove client with id @p client_id from the group and @return the removed client
    ClientInfoPtr removeClient(const std::string& client_id)
    {
        for (auto iter = clients.begin(); iter != clients.end(); ++iter)
        {
            if ((*iter)->id == client_id)
            {
                clients.erase(iter);
                return (*iter);
            }
        }
        return nullptr;
    }

    /// Remove client @p client from the group and @return the removed client
    ClientInfoPtr removeClient(const ClientInfoPtr& client)
    {
        if (!client)
            return nullptr;

        return removeClient(client->id);
    }

    /// @return client with id @p client_id
    ClientInfoPtr getClient(const std::string& client_id)
    {
        for (const auto& client : clients)
        {
            if (client->id == client_id)
                return client;
        }
        return nullptr;
    }

    /// Add client @p client
    void addClient(const ClientInfoPtr& client)
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

    /// @return if the group is empty
    bool empty() const
    {
        return clients.empty();
    }

    std::string name;                   ///< group name
    std::string id;                     ///< group id
    std::string streamId;               ///< stream id assigned to this group
    bool muted{false};                  ///< is the group muted?
    std::vector<ClientInfoPtr> clients; ///< list of clients in this group
};


/// Runtime configuration
class Config
{
public:
    /// singleton c'tor
    static Config& instance()
    {
        static Config instance_;
        return instance_;
    }

    /// @return client from @p client_id
    ClientInfoPtr getClientInfo(const std::string& client_id) const;
    /// Get or create client with id @p client_id and return its group (create a new group for new clients)
    /// @return the client's group
    GroupPtr addClientInfo(const std::string& client_id);
    /// Add client @p client to a group
    /// @return the client's group: exising or newly created
    GroupPtr addClientInfo(const ClientInfoPtr& client);
    /// Remove client @p client from group
    void remove(const ClientInfoPtr& client);
    /// Remove group @group, @p force removal of a non-empty group
    void remove(const GroupPtr& group, bool force = false);

    //	GroupPtr removeFromGroup(const std::string& groupId, const std::string& clientId);
    //	GroupPtr setGroupForClient(const std::string& groupId, const std::string& clientId);

    /// @return grouo from client with id @client_id
    GroupPtr getGroupFromClient(const std::string& client_id);
    /// @return group from @p client
    GroupPtr getGroupFromClient(const ClientInfoPtr& client);
    /// @return group with id @p group_id
    GroupPtr getGroup(const std::string& group_id) const;

    /// @return groups with client as json
    json getGroups() const;
    /// @return complete server status, including @p streams
    json getServerStatus(const json& streams) const;

    /// Save config to file (json format)
    void save();

    /// Set directory and user/group of persistent "server.json"
    void init(const std::string& root_directory = "", const std::string& user = "", const std::string& group = "");

    /// List of groups
    std::vector<GroupPtr> groups;

    /// to protect members
    std::mutex& getMutex();

private:
    /// c'tor
    Config() = default;
    /// d'tor
    ~Config();

    mutable std::recursive_mutex mutex_; ///< to protect members
    std::mutex client_mutex_;            ///< returned by "getMutex()", TODO: check this
    std::string filename_;               ///< filename to persist the config
};
