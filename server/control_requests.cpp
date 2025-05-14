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

// prototype/interface header file
#include "control_requests.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/message/server_settings.hpp"
#include "jsonrpcpp.hpp"
#include "server.hpp"

// 3rd party headers

// standard headers
#include <filesystem>
#include <memory>
#include <tuple>


static constexpr auto LOG_TAG = "ControlRequest";


/// throw InvalidParamsException if one of @p params is missing in @p request
static void checkParams(const jsonrpcpp::request_ptr& request, const std::vector<std::string>& params)
{
    for (const auto& param : params)
    {
        if (!request->params().has(param))
            throw jsonrpcpp::InvalidParamsException("Parameter '" + param + "' is missing", request->id());
    }
}


Request::Request(const Server& server, const std::string& method) : server_(server), method_(method)
{
}

bool Request::hasPermission(const AuthInfo& authinfo) const
{
    std::ignore = authinfo;
    return true;
    // return authinfo.hasPermission(method_);
}

const std::string& Request::method() const
{
    return method_;
}


const StreamServer& Request::getStreamServer() const
{
    return *server_.streamServer_;
}

StreamManager& Request::getStreamManager() const
{
    return *server_.streamManager_;
}

const ServerSettings& Request::getSettings() const
{
    return server_.settings_;
}



ControlRequestFactory::ControlRequestFactory(const Server& server)
{
    auto add_request = [&](std::shared_ptr<Request>&& request) { request_map_[request->method()] = std::move(request); };

    // Client requests
    add_request(std::make_shared<ClientGetStatusRequest>(server));
    add_request(std::make_shared<ClientSetVolumeRequest>(server));
    add_request(std::make_shared<ClientSetLatencyRequest>(server));
    add_request(std::make_shared<ClientSetNameRequest>(server));

    // Group requests
    add_request(std::make_shared<GroupGetStatusRequest>(server));
    add_request(std::make_shared<GroupSetNameRequest>(server));
    add_request(std::make_shared<GroupSetMuteRequest>(server));
    add_request(std::make_shared<GroupSetStreamRequest>(server));
    add_request(std::make_shared<GroupSetClientsRequest>(server));

    // Stream requests
    add_request(std::make_shared<StreamControlRequest>(server));
    add_request(std::make_shared<StreamSetPropertyRequest>(server));
    add_request(std::make_shared<StreamAddRequest>(server));
    add_request(std::make_shared<StreamRemoveRequest>(server));

    // Server requests
    add_request(std::make_shared<ServerGetRpcVersionRequest>(server));
    add_request(std::make_shared<ServerGetStatusRequest>(server));
    add_request(std::make_shared<ServerDeleteClientRequest>(server));
    add_request(std::make_shared<ServerAuthenticateRequest>(server));
    add_request(std::make_shared<ServerGetTokenRequest>(server));
}


std::shared_ptr<Request> ControlRequestFactory::getRequest(const std::string& method) const
{
    auto iter = request_map_.find(method);
    if (iter != request_map_.end())
        return iter->second;
    return nullptr;
}


///////////////////////////////////////// Client requests /////////////////////////////////////////


ClientRequest::ClientRequest(const Server& server, const std::string& method) : Request(server, method)
{
}

ClientInfoPtr ClientRequest::getClient(const jsonrpcpp::request_ptr& request)
{
    checkParams(request, {"id"});

    ClientInfoPtr clientInfo = Config::instance().getClientInfo(request->params().get<std::string>("id"));
    if (clientInfo == nullptr)
        throw jsonrpcpp::InternalErrorException("Client not found", request->id());
    return clientInfo;
}


void ClientRequest::updateClient(const jsonrpcpp::request_ptr& request)
{
    ClientInfoPtr clientInfo = getClient(request);
    session_ptr session = getStreamServer().getStreamSession(clientInfo->id);
    if (session != nullptr)
    {
        auto serverSettings = std::make_shared<msg::ServerSettings>();
        serverSettings->setBufferMs(getSettings().stream.bufferMs);
        serverSettings->setVolume(clientInfo->config.volume.percent);
        GroupPtr group = Config::instance().getGroupFromClient(clientInfo);
        serverSettings->setMuted(clientInfo->config.volume.muted || group->muted);
        serverSettings->setLatency(clientInfo->config.latency);
        session->send(serverSettings);
    }
}


ClientGetStatusRequest::ClientGetStatusRequest(const Server& server) : ClientRequest(server, "Client.GetStatus")
{
}

void ClientGetStatusRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:  {"id":8,"jsonrpc":"2.0","method":"Client.GetStatus","params":{"id":"00:21:6a:7d:74:fc"}}
    // Response: {"id":8,"jsonrpc":"2.0","result":{"client":{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488026416,"usec":135973},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}}}
    // clang-format on

    std::ignore = authinfo;

    Json result;
    result["client"] = getClient(request)->toJson();
    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    on_response(std::move(response), nullptr);
}



ClientSetVolumeRequest::ClientSetVolumeRequest(const Server& server) : ClientRequest(server, "Client.SetVolume")
{
}

void ClientSetVolumeRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":8,"jsonrpc":"2.0","method":"Client.SetVolume","params":{"id":"00:21:6a:7d:74:fc","volume":{"muted":false,"percent":74}}}
    // Response:     {"id":8,"jsonrpc":"2.0","result":{"volume":{"muted":false,"percent":74}}}
    // Notification: {"jsonrpc":"2.0","method":"Client.OnVolumeChanged","params":{"id":"00:21:6a:7d:74:fc","volume":{"muted":false,"percent":74}}}
    // clang-format on

    checkParams(request, {"volume"});

    std::ignore = authinfo;
    auto client_info = getClient(request);
    client_info->config.volume.fromJson(request->params().get("volume"));
    Json result;
    result["volume"] = client_info->config.volume.toJson();

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    auto notification = std::make_shared<jsonrpcpp::Notification>("Client.OnVolumeChanged",
                                                                  jsonrpcpp::Parameter("id", client_info->id, "volume", client_info->config.volume.toJson()));
    on_response(std::move(response), std::move(notification));
    updateClient(request);
}



ClientSetLatencyRequest::ClientSetLatencyRequest(const Server& server) : ClientRequest(server, "Client.SetLatency")
{
}

void ClientSetLatencyRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":7,"jsonrpc":"2.0","method":"Client.SetLatency","params":{"id":"00:21:6a:7d:74:fc#2","latency":10}}
    // Response:     {"id":7,"jsonrpc":"2.0","result":{"latency":10}}
    // Notification: {"jsonrpc":"2.0","method":"Client.OnLatencyChanged","params":{"id":"00:21:6a:7d:74:fc#2","latency":10}}
    // clang-format on

    checkParams(request, {"latency"});

    std::ignore = authinfo;
    int latency = request->params().get("latency");
    if (latency < -10000)
        latency = -10000;
    else if (latency > getSettings().stream.bufferMs)
        latency = getSettings().stream.bufferMs;
    auto client_info = getClient(request);
    client_info->config.latency = latency; //, -10000, settings_.stream.bufferMs);
    Json result;
    result["latency"] = client_info->config.latency;

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    auto notification = std::make_shared<jsonrpcpp::Notification>("Client.OnLatencyChanged",
                                                                  jsonrpcpp::Parameter("id", client_info->id, "latency", client_info->config.latency));
    on_response(std::move(response), std::move(notification));
    updateClient(request);
}



ClientSetNameRequest::ClientSetNameRequest(const Server& server) : ClientRequest(server, "Client.SetName")
{
}

void ClientSetNameRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":6,"jsonrpc":"2.0","method":"Client.SetName","params":{"id":"00:21:6a:7d:74:fc#2","name":"Laptop"}}
    // Response:     {"id":6,"jsonrpc":"2.0","result":{"name":"Laptop"}}
    // Notification: {"jsonrpc":"2.0","method":"Client.OnNameChanged","params":{"id":"00:21:6a:7d:74:fc#2","name":"Laptop"}}
    // clang-format on

    checkParams(request, {"name"});

    std::ignore = authinfo;
    auto client_info = getClient(request);
    client_info->config.name = request->params().get<std::string>("name");
    Json result;
    result["name"] = client_info->config.name;

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    auto notification =
        std::make_shared<jsonrpcpp::Notification>("Client.OnNameChanged", jsonrpcpp::Parameter("id", client_info->id, "name", client_info->config.name));
    on_response(std::move(response), std::move(notification));
    updateClient(request);
}



///////////////////////////////////////// Group requests //////////////////////////////////////////



GroupRequest::GroupRequest(const Server& server, const std::string& method) : Request(server, method)
{
}

GroupPtr GroupRequest::getGroup(const jsonrpcpp::request_ptr& request)
{
    checkParams(request, {"id"});

    GroupPtr group = Config::instance().getGroup(request->params().get<std::string>("id"));
    if (group == nullptr)
        throw jsonrpcpp::InternalErrorException("Group not found", request->id());
    return group;
}



GroupGetStatusRequest::GroupGetStatusRequest(const Server& server) : GroupRequest(server, "Group.GetStatus")
{
}

void GroupGetStatusRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:  {"id":5,"jsonrpc":"2.0","method":"Group.GetStatus","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1"}}
    // Response: {"id":5,"jsonrpc":"2.0","result":{"group":{"clients":[{"config":{"instance":2,"latency":10,"name":"Laptop","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488026485,"usec":644997},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488026481,"usec":223747},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":true,"name":"","stream_id":"stream 1"}}}
    // clang-format on

    std::ignore = authinfo;
    Json result;
    result["group"] = getGroup(request)->toJson();
    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    on_response(std::move(response), nullptr);
}


GroupSetNameRequest::GroupSetNameRequest(const Server& server) : GroupRequest(server, "Group.SetName")
{
}

void GroupSetNameRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":6,"jsonrpc":"2.0","method":"Group.SetName","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","name":"Laptop"}}
    // Response:     {"id":6,"jsonrpc":"2.0","result":{"name":"MediaPlayer"}}
    // Notification: {"jsonrpc":"2.0","method":"Group.OnNameChanged","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","MediaPlayer":"Laptop"}}
    // clang-format on

    checkParams(request, {"name"});

    std::ignore = authinfo;
    Json result;
    auto group = getGroup(request);
    group->name = request->params().get<std::string>("name");
    result["name"] = group->name;

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    auto notification = std::make_shared<jsonrpcpp::Notification>("Group.OnNameChanged", jsonrpcpp::Parameter("id", group->id, "name", group->name));
    on_response(std::move(response), std::move(notification));
}


GroupSetMuteRequest::GroupSetMuteRequest(const Server& server) : GroupRequest(server, "Group.SetMute")
{
}

void GroupSetMuteRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":5,"jsonrpc":"2.0","method":"Group.SetMute","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","mute":true}}
    // Response:     {"id":5,"jsonrpc":"2.0","result":{"mute":true}}
    // Notification: {"jsonrpc":"2.0","method":"Group.OnMute","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","mute":true}}
    // clang-format on

    checkParams(request, {"mute"});

    std::ignore = authinfo;
    bool muted = request->params().get<bool>("mute");
    auto group = getGroup(request);
    group->muted = muted;

    // Update clients
    for (const auto& client : group->clients)
    {
        session_ptr session = getStreamServer().getStreamSession(client->id);
        if (session != nullptr)
        {
            auto serverSettings = std::make_shared<msg::ServerSettings>();
            serverSettings->setBufferMs(getSettings().stream.bufferMs);
            serverSettings->setVolume(client->config.volume.percent);
            GroupPtr group = Config::instance().getGroupFromClient(client);
            serverSettings->setMuted(client->config.volume.muted || group->muted);
            serverSettings->setLatency(client->config.latency);
            session->send(serverSettings);
        }
    }

    Json result;
    result["mute"] = group->muted;

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    auto notification = std::make_shared<jsonrpcpp::Notification>("Group.OnMute", jsonrpcpp::Parameter("id", group->id, "mute", group->muted));
    on_response(std::move(response), std::move(notification));
}


GroupSetStreamRequest::GroupSetStreamRequest(const Server& server) : GroupRequest(server, "Group.SetStream")
{
}

void GroupSetStreamRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":4,"jsonrpc":"2.0","method":"Group.SetStream","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","stream_id":"stream 1"}}
    // Response:     {"id":4,"jsonrpc":"2.0","result":{"stream_id":"stream 1"}}
    // Notification: {"jsonrpc":"2.0","method":"Group.OnStreamChanged","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","stream_id":"stream 1"}}
    // clang-format on

    checkParams(request, {"stream_id"});

    std::ignore = authinfo;
    auto streamId = request->params().get<std::string>("stream_id");
    PcmStreamPtr stream = getStreamManager().getStream(streamId);
    if (stream == nullptr)
        throw jsonrpcpp::InternalErrorException("Stream not found", request->id());

    auto group = getGroup(request);
    group->streamId = streamId;

    // Update clients
    for (const auto& client : group->clients)
    {
        session_ptr session = getStreamServer().getStreamSession(client->id);
        if (session && (session->pcmStream() != stream))
        {
            // session->send(stream->getMeta());
            session->send(stream->getHeader());
            session->setPcmStream(stream);
        }
    }

    // Notify others
    Json result;
    result["stream_id"] = group->streamId;

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    auto notification = std::make_shared<jsonrpcpp::Notification>("Group.OnStreamChanged", jsonrpcpp::Parameter("id", group->id, "stream_id", group->streamId));
    on_response(std::move(response), std::move(notification));
}


GroupSetClientsRequest::GroupSetClientsRequest(const Server& server) : GroupRequest(server, "Group.SetClients")
{
}

void GroupSetClientsRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:  {"id":3,"jsonrpc":"2.0","method":"Group.SetClients","params":{"clients":["00:21:6a:7d:74:fc#2","00:21:6a:7d:74:fc"],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1"}}
    // Response: {"id":3,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025901,"usec":864472},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":100}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025905,"usec":45238},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
    // Notification: {"jsonrpc":"2.0","method":"Server.OnUpdate","params":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025901,"usec":864472},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":100}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025905,"usec":45238},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
    // clang-format on

    checkParams(request, {"clients"});

    std::ignore = authinfo;
    std::vector<std::string> clients = request->params().get("clients");

    // Remove clients from group
    auto group = getGroup(request);
    for (auto iter = group->clients.begin(); iter != group->clients.end();)
    {
        auto client = *iter;
        if (find(clients.begin(), clients.end(), client->id) != clients.end())
        {
            ++iter;
            continue;
        }
        iter = group->clients.erase(iter);
        GroupPtr newGroup = Config::instance().addClientInfo(client);
        newGroup->streamId = group->streamId;
    }

    // Add clients to group
    PcmStreamPtr stream = getStreamManager().getStream(group->streamId);
    for (const auto& clientId : clients)
    {
        ClientInfoPtr client = Config::instance().getClientInfo(clientId);
        if (!client)
            continue;
        GroupPtr oldGroup = Config::instance().getGroupFromClient(client);
        if (oldGroup && (oldGroup->id == group->id))
            continue;

        if (oldGroup)
        {
            oldGroup->removeClient(client);
            Config::instance().remove(oldGroup);
        }

        group->addClient(client);

        // assign new stream
        session_ptr session = getStreamServer().getStreamSession(client->id);
        if (session && stream && (session->pcmStream() != stream))
        {
            // session->send(stream->getMeta());
            session->send(stream->getHeader());
            session->setPcmStream(stream);
        }
    }

    if (group->empty())
        Config::instance().remove(group);

    json server = Config::instance().getServerStatus(getStreamManager().toJson());
    Json result;
    result["server"] = server;

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    // Notify others: since at least two groups are affected, send a complete server update
    auto notification = std::make_shared<jsonrpcpp::Notification>("Server.OnUpdate", jsonrpcpp::Parameter("server", server));
    on_response(std::move(response), std::move(notification));
}



///////////////////////////////////////// Stream requests /////////////////////////////////////////



StreamRequest::StreamRequest(const Server& server, const std::string& method) : Request(server, method)
{
}

PcmStreamPtr StreamRequest::getStream(const StreamManager& stream_manager, const jsonrpcpp::request_ptr& request)
{
    PcmStreamPtr stream = stream_manager.getStream(getStreamId(request));
    if (stream == nullptr)
        throw jsonrpcpp::InternalErrorException("Stream not found", request->id());
    return stream;
}


std::string StreamRequest::getStreamId(const jsonrpcpp::request_ptr& request)
{
    checkParams(request, {"id"});

    return request->params().get<std::string>("id");
}


StreamControlRequest::StreamControlRequest(const Server& server) : StreamRequest(server, "Stream.Control")
{
}

void StreamControlRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.Control","params":{"id":"Spotify", "command": "next", params: {}}}
    // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
    // 
    // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.Control","params":{"id":"Spotify", "command": "seek", "param": "60000"}}
    // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
    // clang-format on

    checkParams(request, {"id", "command"});

    std::ignore = authinfo;
    LOG(INFO, LOG_TAG) << "Stream.Control id: " << request->params().get<std::string>("id") << ", command: " << request->params().get("command")
                       << ", params: " << (request->params().has("params") ? request->params().get("params") : "") << "\n";

    // Find stream
    PcmStreamPtr stream = getStream(getStreamManager(), request);

    auto command = request->params().get<std::string>("command");

    auto handle_response = [request, on_response, command](const snapcast::ErrorCode& ec)
    {
        auto log_level = AixLog::Severity::debug;
        if (ec)
            log_level = AixLog::Severity::error;
        LOG(log_level, LOG_TAG) << "Response to '" << command << "': " << ec << ", message: " << ec.detailed_message() << ", msg: " << ec.message()
                                << ", category: " << ec.category().name() << "\n";
        std::shared_ptr<jsonrpcpp::Response> response;
        if (ec)
            response = std::make_shared<jsonrpcpp::Response>(request->id(), jsonrpcpp::Error(ec.detailed_message(), ec.value()));
        else
            response = std::make_shared<jsonrpcpp::Response>(request->id(), "ok");
        // LOG(DEBUG, LOG_TAG) << response->to_json().dump() << "\n";
        on_response(std::move(response), nullptr);
    };

    if (command == "setPosition")
    {
        if (!request->params().has("params") || !request->params().get("params").contains("position"))
            throw jsonrpcpp::InvalidParamsException("setPosition requires parameter 'position'");
        auto seconds = request->params().get("params")["position"].get<float>();
        stream->setPosition(std::chrono::milliseconds(static_cast<int>(seconds * 1000)),
                            [handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
    }
    else if (command == "seek")
    {
        if (!request->params().has("params") || !request->params().get("params").contains("offset"))
            throw jsonrpcpp::InvalidParamsException("Seek requires parameter 'offset'");
        auto offset = request->params().get("params")["offset"].get<float>();
        stream->seek(std::chrono::milliseconds(static_cast<int>(offset * 1000)), [handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
    }
    else if (command == "next")
    {
        stream->next([handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
    }
    else if (command == "previous")
    {
        stream->previous([handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
    }
    else if (command == "pause")
    {
        stream->pause([handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
    }
    else if (command == "playPause")
    {
        stream->playPause([handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
    }
    else if (command == "stop")
    {
        stream->stop([handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
    }
    else if (command == "play")
    {
        stream->play([handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
    }
    else
        throw jsonrpcpp::InvalidParamsException("Command '" + command + "' not supported", request->id());
}


StreamSetPropertyRequest::StreamSetPropertyRequest(const Server& server) : StreamRequest(server, "Stream.SetProperty")
{
}

void StreamSetPropertyRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    checkParams(request, {"property", "value", "id"});

    LOG(INFO, LOG_TAG) << "Stream.SetProperty id: " << request->params().get<std::string>("id") << ", property: " << request->params().get("property")
                       << ", value: " << request->params().get("value") << "\n";

    std::ignore = authinfo;
    // Find stream
    std::string streamId = getStreamId(request);
    PcmStreamPtr stream = getStream(getStreamManager(), request);

    auto name = request->params().get<std::string>("property");
    auto value = request->params().get("value");
    LOG(INFO, LOG_TAG) << "Stream '" << streamId << "' set property: " << name << " = " << value << "\n";

    auto handle_response = [request, on_response](const std::string& command, const snapcast::ErrorCode& ec)
    {
        LOG(ERROR, LOG_TAG) << "Result for '" << command << "': " << ec << ", message: " << ec.detailed_message() << ", msg: " << ec.message()
                            << ", category: " << ec.category().name() << "\n";
        std::shared_ptr<jsonrpcpp::Response> response;
        if (ec)
            response = std::make_shared<jsonrpcpp::Response>(request->id(), jsonrpcpp::Error(ec.detailed_message(), ec.value()));
        else
            response = std::make_shared<jsonrpcpp::Response>(request->id(), "ok");
        on_response(response, nullptr);
    };

    if (name == "loopStatus")
    {
        auto val = value.get<std::string>();
        LoopStatus loop_status = loop_status_from_string(val);
        if (loop_status == LoopStatus::kUnknown)
            throw jsonrpcpp::InvalidParamsException("Value for loopStatus must be one of 'none', 'track', 'playlist'", request->id());
        stream->setLoopStatus(loop_status, [handle_response, name](const snapcast::ErrorCode& ec) { handle_response(name, ec); });
    }
    else if (name == "shuffle")
    {
        if (!value.is_boolean())
            throw jsonrpcpp::InvalidParamsException("Value for shuffle must be bool", request->id());
        stream->setShuffle(value.get<bool>(), [handle_response, name](const snapcast::ErrorCode& ec) { handle_response(name, ec); });
    }
    else if (name == "volume")
    {
        if (!value.is_number_integer())
            throw jsonrpcpp::InvalidParamsException("Value for volume must be an int", request->id());
        stream->setVolume(value.get<int16_t>(), [handle_response, name](const snapcast::ErrorCode& ec) { handle_response(name, ec); });
    }
    else if (name == "mute")
    {
        if (!value.is_boolean())
            throw jsonrpcpp::InvalidParamsException("Value for mute must be bool", request->id());
        stream->setMute(value.get<bool>(), [handle_response, name](const snapcast::ErrorCode& ec) { handle_response(name, ec); });
    }
    else if (name == "rate")
    {
        if (!value.is_number_float())
            throw jsonrpcpp::InvalidParamsException("Value for rate must be float", request->id());
        stream->setRate(value.get<float>(), [handle_response, name](const snapcast::ErrorCode& ec) { handle_response(name, ec); });
    }
    else
        throw jsonrpcpp::InvalidParamsException("Property '" + name + "' not supported", request->id());
}


StreamAddRequest::StreamAddRequest(const Server& server) : StreamRequest(server, "Stream.AddStream")
{
}

void StreamAddRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.AddStream","params":{"streamUri":"uri"}}
    // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
    // clang-format on

    checkParams(request, {"streamUri"});

    // Don't allow adding streams that start a user defined process: CVE-2023-36177
    static constexpr std::array whitelist{"pipe", "file", "tcp", "alsa", "jack", "meta"};
    std::string stream_uri = request->params().get("streamUri");
    StreamUri parsed_uri(stream_uri);

    if (std::find(whitelist.begin(), whitelist.end(), parsed_uri.scheme) == whitelist.end())
        throw jsonrpcpp::InvalidParamsException("Adding '" + parsed_uri.scheme + "' streams is not allowed", request->id());

    std::filesystem::path script = parsed_uri.getQuery("controlscript");
    if (!script.empty())
    {
        // script must be located in the [stream] plugin_dir
        std::filesystem::path plugin_dir = getSettings().stream.plugin_dir;
        // if script file name is relative, prepend the plugin_dir
        if (!script.is_absolute())
            script = plugin_dir / script;
        // convert to normalized absolute path
        script = std::filesystem::weakly_canonical(script);
        LOG(DEBUG, LOG_TAG) << "controlscript: " << script.native() << "\n";
        // check if script is directly located in plugin_dir
        if (script.parent_path() != plugin_dir)
            throw jsonrpcpp::InvalidParamsException("controlscript must be located in '" + plugin_dir.native() + "'");
        if (!std::filesystem::exists(script))
            throw jsonrpcpp::InvalidParamsException("controlscript '" + script.native() + "' does not exist");
        parsed_uri.query["controlscript"] = script;
        LOG(DEBUG, LOG_TAG) << "Raw stream uri: " << stream_uri << "\n";
        stream_uri = parsed_uri.toString();
    }

    std::ignore = authinfo;
    LOG(INFO, LOG_TAG) << "Stream.AddStream(" << stream_uri << ")\n";

    // Add stream
    PcmStreamPtr stream = getStreamManager().addStream(stream_uri);
    if (stream == nullptr)
        throw jsonrpcpp::InternalErrorException("Stream not created", request->id());
    stream->start(); // We start the stream, otherwise it would be silent

    // Setup response
    Json result;
    result["id"] = stream->getId();

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    on_response(std::move(response), nullptr);
}


StreamRemoveRequest::StreamRemoveRequest(const Server& server) : StreamRequest(server, "Stream.RemoveStream")
{
}

void StreamRemoveRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.RemoveStream","params":{"id":"Spotify"}}
    // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
    // clang-format on
    checkParams(request, {"id"});

    std::ignore = authinfo;
    LOG(INFO, LOG_TAG) << "Stream.RemoveStream(" << request->params().get("id") << ")\n";

    // Find stream
    std::string streamId = getStreamId(request);
    getStreamManager().removeStream(streamId);

    // Setup response
    Json result;
    result["id"] = streamId;
    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    on_response(std::move(response), nullptr);
}



///////////////////////////////////////// Server requests /////////////////////////////////////////



ServerGetRpcVersionRequest::ServerGetRpcVersionRequest(const Server& server) : Request(server, "Server.GetRPCVersion")
{
}

void ServerGetRpcVersionRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":8,"jsonrpc":"2.0","method":"Server.GetRPCVersion"}
    // Response:     {"id":8,"jsonrpc":"2.0","result":{"major":2,"minor":0,"patch":0}}
    // clang-format on

    std::ignore = authinfo;
    Json result;
    // <major>: backwards incompatible change
    result["major"] = 23;
    // <minor>: feature addition to the API
    result["minor"] = 0;
    // <patch>: bugfix release
    result["patch"] = 0;
    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    on_response(std::move(response), nullptr);
}



ServerGetStatusRequest::ServerGetStatusRequest(const Server& server) : Request(server, "Server.GetStatus")
{
}

void ServerGetStatusRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":1,"jsonrpc":"2.0","method":"Server.GetStatus"}
    // Response:     {"id":1,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025696,"usec":578142},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":81}},"connected":true,"host":{"arch":"x86_64","ip":"192.168.0.54","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025696,"usec":611255},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
    // clang-format on

    std::ignore = authinfo;
    Json result;
    result["server"] = Config::instance().getServerStatus(getStreamManager().toJson());
    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    on_response(std::move(response), nullptr);
}



ServerDeleteClientRequest::ServerDeleteClientRequest(const Server& server) : Request(server, "Server.DeleteClient")
{
}

void ServerDeleteClientRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":2,"jsonrpc":"2.0","method":"Server.DeleteClient","params":{"id":"00:21:6a:7d:74:fc"}}
    // Response:     {"id":2,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025751,"usec":654777},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
    // Notification: {"jsonrpc":"2.0","method":"Server.OnUpdate","params":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025751,"usec":654777},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
    // clang-format on

    checkParams(request, {"id"});

    std::ignore = authinfo;
    ClientInfoPtr clientInfo = Config::instance().getClientInfo(request->params().get<std::string>("id"));
    if (clientInfo == nullptr)
        throw jsonrpcpp::InternalErrorException("Client not found", request->id());

    Config::instance().remove(clientInfo);

    json server = Config::instance().getServerStatus(getStreamManager().toJson());
    Json result;
    result["server"] = server;

    auto response = std::make_shared<jsonrpcpp::Response>(*request, result);
    auto notification = std::make_shared<jsonrpcpp::Notification>("Server.OnUpdate", jsonrpcpp::Parameter("server", server));
    on_response(std::move(response), std::move(notification));
}


ServerAuthenticateRequest::ServerAuthenticateRequest(const Server& server) : Request(server, "Server.Authenticate")
{
}

void ServerAuthenticateRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":8,"jsonrpc":"2.0","method":"Server.Authenticate","params":{"scheme":"Basic","param":"YmFkYWl4OnNlY3JldA=="}}
    // Response:     {"id":8,"jsonrpc":"2.0","result":"ok"}
    // Request:      {"id":8,"jsonrpc":"2.0","method":"Server.Authenticate","params":{"scheme":"Bearer","param":"eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE3MTg1NjQ1MTYsImlhdCI6MTcxODUyODUxNiwic3ViIjoiQmFkYWl4In0.gHrMVp7jTAg8aCSg3cttcfIxswqmOPuqVNOb5p79Cn0NmAqRmLXtDLX4QjOoOqqb66ezBBeikpNjPi_aO18YPoNmX9fPxSwcObTHBupnm5eugEpneMPDASFUSE2hg8rrD_OEoAVxx6hCLln7Z3ILyWDmR6jcmy7z0bp0BiAqOywUrFoVIsnlDZRs3wOaap5oS9J2oaA_gNi_7OuvAhrydn26LDhm0KiIqEcyIholkpRHrDYODkz98h2PkZdZ2U429tTvVhzDBJ1cBq2Zq3cvuMZT6qhwaUc8eYA8fUJ7g65iP4o2OZtUzlfEUqX1TKyuWuSK6CUlsZooNE-MSCT7_w"}}
    // Response:     {"id":8,"jsonrpc":"2.0","result":"ok"}
    // clang-format on

    checkParams(request, {"scheme", "param"});

    auto scheme = request->params().get<std::string>("scheme");
    auto param = request->params().get<std::string>("param");
    LOG(INFO, LOG_TAG) << "Authorization scheme: " << scheme << ", param: " << param << "\n";
    auto ec = authinfo.authenticate(scheme, param);

    std::shared_ptr<jsonrpcpp::Response> response;
    if (ec)
        response = std::make_shared<jsonrpcpp::Response>(request->id(), jsonrpcpp::Error(ec.detailed_message(), ec.value()));
    else
        response = std::make_shared<jsonrpcpp::Response>(request->id(), "ok");
    // LOG(DEBUG, LOG_TAG) << response->to_json().dump() << "\n";

    on_response(std::move(response), nullptr);
}


ServerGetTokenRequest::ServerGetTokenRequest(const Server& server) : Request(server, "Server.GetToken")
{
}

void ServerGetTokenRequest::execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response)
{
    // clang-format off
    // Request:      {"id":8,"jsonrpc":"2.0","method":"Server.GetToken","params":{"username":"Badaix","password":"secret"}}
    // Response:     {"id":8,"jsonrpc":"2.0","result":{"token":"<token>"}}
    // clang-format on

    checkParams(request, {"username", "password"});

    auto username = request->params().get<std::string>("username");
    auto password = request->params().get<std::string>("password");
    LOG(INFO, LOG_TAG) << "GetToken username: " << username << ", password: " << password << "\n";
    auto token = authinfo.getToken(username, password);

    std::shared_ptr<jsonrpcpp::Response> response;
    if (token.hasError())
    {
        response = std::make_shared<jsonrpcpp::Response>(request->id(), jsonrpcpp::Error(token.getError().detailed_message(), token.getError().value()));
    }
    else
    {
        Json result;
        result["token"] = token.takeValue();
        response = std::make_shared<jsonrpcpp::Response>(*request, result);
    }
    // LOG(DEBUG, LOG_TAG) << response->to_json().dump() << "\n";

    on_response(std::move(response), nullptr);
}
