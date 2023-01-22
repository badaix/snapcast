/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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
#include "server.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/message/client_info.hpp"
#include "common/message/hello.hpp"
#include "common/message/time.hpp"
#include "config.hpp"
#include "stream_session_tcp.hpp"

// 3rd party headers

// standard headers
#include <iostream>

using namespace std;
using namespace streamreader;

using json = nlohmann::json;

static constexpr auto LOG_TAG = "Server";

Server::Server(boost::asio::io_context& io_context, const ServerSettings& serverSettings)
    : io_context_(io_context), config_timer_(io_context), settings_(serverSettings)
{
}


Server::~Server() = default;


void Server::onNewSession(std::shared_ptr<StreamSession> session)
{
    LOG(DEBUG, LOG_TAG) << "onNewSession\n";
    streamServer_->addSession(std::move(session));
}


void Server::onPropertiesChanged(const PcmStream* pcmStream, const Properties& properties)
{
    LOG(DEBUG, LOG_TAG) << "Properties changed, stream: " << pcmStream->getName() << ", properties: " << properties.toJson().dump(3) << "\n";

    // Send properties to all connected control clients
    json notification =
        jsonrpcpp::Notification("Stream.OnProperties", jsonrpcpp::Parameter("id", pcmStream->getId(), "properties", properties.toJson())).to_json();
    controlServer_->send(notification.dump(), nullptr);
}


void Server::onStateChanged(const PcmStream* pcmStream, ReaderState state)
{
    // clang-format off
    // Notification: {"jsonrpc":"2.0","method":"Stream.OnUpdate","params":{"id":"stream 1","stream":{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}}}}
    // clang-format on
    LOG(INFO, LOG_TAG) << "onStateChanged (" << pcmStream->getName() << "): " << state << "\n";
    //	LOG(INFO, LOG_TAG) << pcmStream->toJson().dump(4);
    json notification = jsonrpcpp::Notification("Stream.OnUpdate", jsonrpcpp::Parameter("id", pcmStream->getId(), "stream", pcmStream->toJson())).to_json();
    controlServer_->send(notification.dump(), nullptr);
    // cout << "Notification: " << notification.dump() << "\n";
}


void Server::onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk)
{
    std::ignore = pcmStream;
    std::ignore = chunk;
}


void Server::onChunkEncoded(const PcmStream* pcmStream, std::shared_ptr<msg::PcmChunk> chunk, double duration)
{
    streamServer_->onChunkEncoded(pcmStream, pcmStream == streamManager_->getDefaultStream().get(), chunk, duration);
}


void Server::onResync(const PcmStream* pcmStream, double ms)
{
    LOG(INFO, LOG_TAG) << "onResync (" << pcmStream->getName() << "): " << ms << " ms\n";
}


void Server::onDisconnect(StreamSession* streamSession)
{
    // notify controllers if not yet done
    ClientInfoPtr clientInfo = Config::instance().getClientInfo(streamSession->clientId);
    if (!clientInfo || !clientInfo->connected)
        return;

    clientInfo->connected = false;
    chronos::systemtimeofday(&clientInfo->lastSeen);
    saveConfig();
    if (controlServer_ != nullptr)
    {
        // Check if there is no session of this client left
        // Can happen in case of ungraceful disconnect/reconnect or
        // in case of a duplicate client id
        if (streamServer_->getStreamSession(clientInfo->id) == nullptr)
        {
            // clang-format off
            // Notification:
            // {"jsonrpc":"2.0","method":"Client.OnDisconnect","params":{"client":{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":81}},"connected":false,"host":{"arch":"x86_64","ip":"192.168.0.54","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025523,"usec":814067},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},"id":"00:21:6a:7d:74:fc"}}
            // clang-format on
            json notification =
                jsonrpcpp::Notification("Client.OnDisconnect", jsonrpcpp::Parameter("id", clientInfo->id, "client", clientInfo->toJson())).to_json();
            controlServer_->send(notification.dump());
            // cout << "Notification: " << notification.dump() << "\n";
        }
    }
}


void Server::processRequest(const jsonrpcpp::request_ptr request, const OnResponse& on_response) const
{
    jsonrpcpp::entity_ptr response;
    jsonrpcpp::notification_ptr notification;
    try
    {
        // LOG(INFO, LOG_TAG) << "Server::processRequest method: " << request->method << ", " << "id: " << request->id() << "\n";
        Json result;

        if (request->method().find("Client.") == 0)
        {
            ClientInfoPtr clientInfo = Config::instance().getClientInfo(request->params().get<std::string>("id"));
            if (clientInfo == nullptr)
                throw jsonrpcpp::InternalErrorException("Client not found", request->id());

            if (request->method() == "Client.GetStatus")
            {
                // clang-format off
                // Request:  {"id":8,"jsonrpc":"2.0","method":"Client.GetStatus","params":{"id":"00:21:6a:7d:74:fc"}}
                // Response: {"id":8,"jsonrpc":"2.0","result":{"client":{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488026416,"usec":135973},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}}}
                // clang-format on
                result["client"] = clientInfo->toJson();
            }
            else if (request->method() == "Client.SetVolume")
            {
                // clang-format off
                // Request:      {"id":8,"jsonrpc":"2.0","method":"Client.SetVolume","params":{"id":"00:21:6a:7d:74:fc","volume":{"muted":false,"percent":74}}}
                // Response:     {"id":8,"jsonrpc":"2.0","result":{"volume":{"muted":false,"percent":74}}}
                // Notification: {"jsonrpc":"2.0","method":"Client.OnVolumeChanged","params":{"id":"00:21:6a:7d:74:fc","volume":{"muted":false,"percent":74}}}
                // clang-format on

                clientInfo->config.volume.fromJson(request->params().get("volume"));
                result["volume"] = clientInfo->config.volume.toJson();
                notification = std::make_shared<jsonrpcpp::Notification>(
                    "Client.OnVolumeChanged", jsonrpcpp::Parameter("id", clientInfo->id, "volume", clientInfo->config.volume.toJson()));
            }
            else if (request->method() == "Client.SetLatency")
            {
                // clang-format off
                // Request:      {"id":7,"jsonrpc":"2.0","method":"Client.SetLatency","params":{"id":"00:21:6a:7d:74:fc#2","latency":10}}
                // Response:     {"id":7,"jsonrpc":"2.0","result":{"latency":10}}
                // Notification: {"jsonrpc":"2.0","method":"Client.OnLatencyChanged","params":{"id":"00:21:6a:7d:74:fc#2","latency":10}}
                // clang-format on
                int latency = request->params().get("latency");
                if (latency < -10000)
                    latency = -10000;
                else if (latency > settings_.stream.bufferMs)
                    latency = settings_.stream.bufferMs;
                clientInfo->config.latency = latency; //, -10000, settings_.stream.bufferMs);
                result["latency"] = clientInfo->config.latency;
                notification = std::make_shared<jsonrpcpp::Notification>("Client.OnLatencyChanged",
                                                                         jsonrpcpp::Parameter("id", clientInfo->id, "latency", clientInfo->config.latency));
            }
            else if (request->method() == "Client.SetName")
            {
                // clang-format off
                // Request:      {"id":6,"jsonrpc":"2.0","method":"Client.SetName","params":{"id":"00:21:6a:7d:74:fc#2","name":"Laptop"}}
                // Response:     {"id":6,"jsonrpc":"2.0","result":{"name":"Laptop"}}
                // Notification: {"jsonrpc":"2.0","method":"Client.OnNameChanged","params":{"id":"00:21:6a:7d:74:fc#2","name":"Laptop"}}
                // clang-format on
                clientInfo->config.name = request->params().get<std::string>("name");
                result["name"] = clientInfo->config.name;
                notification = std::make_shared<jsonrpcpp::Notification>("Client.OnNameChanged",
                                                                         jsonrpcpp::Parameter("id", clientInfo->id, "name", clientInfo->config.name));
            }
            else
                throw jsonrpcpp::MethodNotFoundException(request->id());


            if (request->method().find("Client.Set") == 0)
            {
                /// Update client
                session_ptr session = streamServer_->getStreamSession(clientInfo->id);
                if (session != nullptr)
                {
                    auto serverSettings = make_shared<msg::ServerSettings>();
                    serverSettings->setBufferMs(settings_.stream.bufferMs);
                    serverSettings->setVolume(clientInfo->config.volume.percent);
                    GroupPtr group = Config::instance().getGroupFromClient(clientInfo);
                    serverSettings->setMuted(clientInfo->config.volume.muted || group->muted);
                    serverSettings->setLatency(clientInfo->config.latency);
                    session->send(serverSettings);
                }
            }
        }
        else if (request->method().find("Group.") == 0)
        {
            GroupPtr group = Config::instance().getGroup(request->params().get<std::string>("id"));
            if (group == nullptr)
                throw jsonrpcpp::InternalErrorException("Group not found", request->id());

            if (request->method() == "Group.GetStatus")
            {
                // clang-format off
                // Request:  {"id":5,"jsonrpc":"2.0","method":"Group.GetStatus","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1"}}
                // Response: {"id":5,"jsonrpc":"2.0","result":{"group":{"clients":[{"config":{"instance":2,"latency":10,"name":"Laptop","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488026485,"usec":644997},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":74}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488026481,"usec":223747},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":true,"name":"","stream_id":"stream 1"}}}
                // clang-format on
                result["group"] = group->toJson();
            }
            else if (request->method() == "Group.SetName")
            {
                // clang-format off
                // Request:      {"id":6,"jsonrpc":"2.0","method":"Group.SetName","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","name":"Laptop"}}
                // Response:     {"id":6,"jsonrpc":"2.0","result":{"name":"MediaPlayer"}}
                // Notification: {"jsonrpc":"2.0","method":"Group.OnNameChanged","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","MediaPlayer":"Laptop"}}
                // clang-format on
                group->name = request->params().get<std::string>("name");
                result["name"] = group->name;
                notification = std::make_shared<jsonrpcpp::Notification>("Group.OnNameChanged", jsonrpcpp::Parameter("id", group->id, "name", group->name));
            }
            else if (request->method() == "Group.SetMute")
            {
                // clang-format off
                // Request:      {"id":5,"jsonrpc":"2.0","method":"Group.SetMute","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","mute":true}}
                // Response:     {"id":5,"jsonrpc":"2.0","result":{"mute":true}}
                // Notification: {"jsonrpc":"2.0","method":"Group.OnMute","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","mute":true}}
                // clang-format on
                bool muted = request->params().get<bool>("mute");
                group->muted = muted;

                /// Update clients
                for (const auto& client : group->clients)
                {
                    session_ptr session = streamServer_->getStreamSession(client->id);
                    if (session != nullptr)
                    {
                        auto serverSettings = make_shared<msg::ServerSettings>();
                        serverSettings->setBufferMs(settings_.stream.bufferMs);
                        serverSettings->setVolume(client->config.volume.percent);
                        GroupPtr group = Config::instance().getGroupFromClient(client);
                        serverSettings->setMuted(client->config.volume.muted || group->muted);
                        serverSettings->setLatency(client->config.latency);
                        session->send(serverSettings);
                    }
                }

                result["mute"] = group->muted;
                notification = std::make_shared<jsonrpcpp::Notification>("Group.OnMute", jsonrpcpp::Parameter("id", group->id, "mute", group->muted));
            }
            else if (request->method() == "Group.SetStream")
            {
                // clang-format off
                // Request:      {"id":4,"jsonrpc":"2.0","method":"Group.SetStream","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","stream_id":"stream 1"}}
                // Response:     {"id":4,"jsonrpc":"2.0","result":{"stream_id":"stream 1"}}
                // Notification: {"jsonrpc":"2.0","method":"Group.OnStreamChanged","params":{"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","stream_id":"stream 1"}}
                // clang-format on
                string streamId = request->params().get<std::string>("stream_id");
                PcmStreamPtr stream = streamManager_->getStream(streamId);
                if (stream == nullptr)
                    throw jsonrpcpp::InternalErrorException("Stream not found", request->id());

                group->streamId = streamId;

                // Update clients
                for (const auto& client : group->clients)
                {
                    session_ptr session = streamServer_->getStreamSession(client->id);
                    if (session && (session->pcmStream() != stream))
                    {
                        // session->send(stream->getMeta());
                        session->send(stream->getHeader());
                        session->setPcmStream(stream);
                    }
                }

                // Notify others
                result["stream_id"] = group->streamId;
                notification =
                    std::make_shared<jsonrpcpp::Notification>("Group.OnStreamChanged", jsonrpcpp::Parameter("id", group->id, "stream_id", group->streamId));
            }
            else if (request->method() == "Group.SetClients")
            {
                // clang-format off
                // Request:  {"id":3,"jsonrpc":"2.0","method":"Group.SetClients","params":{"clients":["00:21:6a:7d:74:fc#2","00:21:6a:7d:74:fc"],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1"}}
                // Response: {"id":3,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025901,"usec":864472},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":100}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025905,"usec":45238},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
                // Notification: {"jsonrpc":"2.0","method":"Server.OnUpdate","params":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025901,"usec":864472},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":100}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025905,"usec":45238},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
                // clang-format on
                vector<string> clients = request->params().get("clients");
                // Remove clients from group
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
                PcmStreamPtr stream = streamManager_->getStream(group->streamId);
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
                    session_ptr session = streamServer_->getStreamSession(client->id);
                    if (session && stream && (session->pcmStream() != stream))
                    {
                        // session->send(stream->getMeta());
                        session->send(stream->getHeader());
                        session->setPcmStream(stream);
                    }
                }

                if (group->empty())
                    Config::instance().remove(group);

                json server = Config::instance().getServerStatus(streamManager_->toJson());
                result["server"] = server;

                // Notify others: since at least two groups are affected, send a complete server update
                notification = std::make_shared<jsonrpcpp::Notification>("Server.OnUpdate", jsonrpcpp::Parameter("server", server));
            }
            else
                throw jsonrpcpp::MethodNotFoundException(request->id());
        }
        else if (request->method().find("Server.") == 0)
        {
            if (request->method().find("Server.GetRPCVersion") == 0)
            {
                // Request:      {"id":8,"jsonrpc":"2.0","method":"Server.GetRPCVersion"}
                // Response:     {"id":8,"jsonrpc":"2.0","result":{"major":2,"minor":0,"patch":0}}
                // <major>: backwards incompatible change
                result["major"] = 2;
                // <minor>: feature addition to the API
                result["minor"] = 0;
                // <patch>: bugfix release
                result["patch"] = 0;
            }
            else if (request->method() == "Server.GetStatus")
            {
                // clang-format off
                // Request:      {"id":1,"jsonrpc":"2.0","method":"Server.GetStatus"}
                // Response:     {"id":1,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025696,"usec":578142},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":81}},"connected":true,"host":{"arch":"x86_64","ip":"192.168.0.54","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025696,"usec":611255},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
                // clang-format on
                result["server"] = Config::instance().getServerStatus(streamManager_->toJson());
            }
            else if (request->method() == "Server.DeleteClient")
            {
                // clang-format off
                // Request:      {"id":2,"jsonrpc":"2.0","method":"Server.DeleteClient","params":{"id":"00:21:6a:7d:74:fc"}}
                // Response:     {"id":2,"jsonrpc":"2.0","result":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025751,"usec":654777},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
                // Notification: {"jsonrpc":"2.0","method":"Server.OnUpdate","params":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025751,"usec":654777},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
                // clang-format on
                ClientInfoPtr clientInfo = Config::instance().getClientInfo(request->params().get<std::string>("id"));
                if (clientInfo == nullptr)
                    throw jsonrpcpp::InternalErrorException("Client not found", request->id());

                Config::instance().remove(clientInfo);

                json server = Config::instance().getServerStatus(streamManager_->toJson());
                result["server"] = server;

                /// Notify others
                notification = std::make_shared<jsonrpcpp::Notification>("Server.OnUpdate", jsonrpcpp::Parameter("server", server));
            }
            else
                throw jsonrpcpp::MethodNotFoundException(request->id());
        }
        else if (request->method().find("Stream.") == 0)
        {
            // if (request->method().find("Stream.SetMeta") == 0)
            // {
            // clang-format off
                // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.SetMeta","params":{"id":"Spotify", "metadata": {"album": "some album", "artist": "some artist", "track": "some track"...}}}
                // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
            // clang-format on

            //     LOG(INFO, LOG_TAG) << "Stream.SetMeta id: " << request->params().get<std::string>("id") << ", meta: " << request->params().get("metadata") <<
            //     "\n";

            //     // Find stream
            //     string streamId = request->params().get<std::string>("id");
            //     PcmStreamPtr stream = streamManager_->getStream(streamId);
            //     if (stream == nullptr)
            //         throw jsonrpcpp::InternalErrorException("Stream not found", request->id());

            //     // Set metadata from request
            //     stream->setMetadata(request->params().get("metadata"));

            //     // Setup response
            //     result["id"] = streamId;
            // }
            if (request->method().find("Stream.Control") == 0)
            {
                // clang-format off
                // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.Control","params":{"id":"Spotify", "command": "next", params: {}}}
                // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
                // 
                // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.Control","params":{"id":"Spotify", "command": "seek", "param": "60000"}}
                // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
                // clang-format on

                LOG(INFO, LOG_TAG) << "Stream.Control id: " << request->params().get<std::string>("id") << ", command: " << request->params().get("command")
                                   << ", params: " << (request->params().has("params") ? request->params().get("params") : "") << "\n";

                // Find stream
                string streamId = request->params().get<std::string>("id");
                PcmStreamPtr stream = streamManager_->getStream(streamId);
                if (stream == nullptr)
                    throw jsonrpcpp::InternalErrorException("Stream not found", request->id());

                if (!request->params().has("command"))
                    throw jsonrpcpp::InvalidParamsException("Parameter 'commmand' is missing", request->id());

                auto command = request->params().get<string>("command");

                auto handle_response = [request, on_response, command](const snapcast::ErrorCode& ec)
                {
                    auto log_level = AixLog::Severity::debug;
                    if (ec)
                        log_level = AixLog::Severity::error;
                    LOG(log_level, LOG_TAG) << "Response to '" << command << "': " << ec << ", message: " << ec.detailed_message() << ", msg: " << ec.message()
                                            << ", category: " << ec.category().name() << "\n";
                    std::shared_ptr<jsonrpcpp::Response> response;
                    if (ec)
                        response = make_shared<jsonrpcpp::Response>(request->id(), jsonrpcpp::Error(ec.detailed_message(), ec.value()));
                    else
                        response = make_shared<jsonrpcpp::Response>(request->id(), "ok");
                    // LOG(DEBUG, LOG_TAG) << response->to_json().dump() << "\n";
                    on_response(response, nullptr);
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
                    stream->seek(std::chrono::milliseconds(static_cast<int>(offset * 1000)),
                                 [handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
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

                return;
            }
            else if (request->method().find("Stream.SetProperty") == 0)
            {
                LOG(INFO, LOG_TAG) << "Stream.SetProperty id: " << request->params().get<std::string>("id")
                                   << ", property: " << request->params().get("property") << ", value: " << request->params().get("value") << "\n";

                // Find stream
                string streamId = request->params().get<std::string>("id");
                PcmStreamPtr stream = streamManager_->getStream(streamId);
                if (stream == nullptr)
                    throw jsonrpcpp::InternalErrorException("Stream not found", request->id());

                if (!request->params().has("property"))
                    throw jsonrpcpp::InvalidParamsException("Parameter 'property' is missing", request->id());

                if (!request->params().has("value"))
                    throw jsonrpcpp::InvalidParamsException("Parameter 'value' is missing", request->id());

                auto name = request->params().get<string>("property");
                auto value = request->params().get("value");
                LOG(INFO, LOG_TAG) << "Stream '" << streamId << "' set property: " << name << " = " << value << "\n";

                auto handle_response = [request, on_response](const snapcast::ErrorCode& ec)
                {
                    LOG(ERROR, LOG_TAG) << "SetShuffle: " << ec << ", message: " << ec.detailed_message() << ", msg: " << ec.message()
                                        << ", category: " << ec.category().name() << "\n";
                    std::shared_ptr<jsonrpcpp::Response> response;
                    if (ec)
                        response = make_shared<jsonrpcpp::Response>(request->id(), jsonrpcpp::Error(ec.detailed_message(), ec.value()));
                    else
                        response = make_shared<jsonrpcpp::Response>(request->id(), "ok");
                    on_response(response, nullptr);
                };

                if (name == "loopStatus")
                {
                    auto val = value.get<std::string>();
                    LoopStatus loop_status = loop_status_from_string(val);
                    if (loop_status == LoopStatus::kUnknown)
                        throw jsonrpcpp::InvalidParamsException("Value for loopStatus must be one of 'none', 'track', 'playlist'", request->id());
                    stream->setLoopStatus(loop_status, [handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
                }
                else if (name == "shuffle")
                {
                    if (!value.is_boolean())
                        throw jsonrpcpp::InvalidParamsException("Value for shuffle must be bool", request->id());
                    stream->setShuffle(value.get<bool>(), [handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
                }
                else if (name == "volume")
                {
                    if (!value.is_number_integer())
                        throw jsonrpcpp::InvalidParamsException("Value for volume must be an int", request->id());
                    stream->setVolume(value.get<int16_t>(), [handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
                }
                else if (name == "mute")
                {
                    if (!value.is_boolean())
                        throw jsonrpcpp::InvalidParamsException("Value for mute must be bool", request->id());
                    stream->setMute(value.get<bool>(), [handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
                }
                else if (name == "rate")
                {
                    if (!value.is_number_float())
                        throw jsonrpcpp::InvalidParamsException("Value for rate must be float", request->id());
                    stream->setRate(value.get<float>(), [handle_response](const snapcast::ErrorCode& ec) { handle_response(ec); });
                }
                else
                    throw jsonrpcpp::InvalidParamsException("Property '" + name + "' not supported", request->id());

                return;
            }
            else if (request->method() == "Stream.AddStream")
            {
                // clang-format off
                // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.AddStream","params":{"streamUri":"uri"}}
                // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
                // clang-format on

                LOG(INFO, LOG_TAG) << "Stream.AddStream(" << request->params().get("streamUri") << ")"
                                   << "\n";

                // Find stream
                string streamUri = request->params().get("streamUri");
                PcmStreamPtr stream = streamManager_->addStream(streamUri);
                if (stream == nullptr)
                    throw jsonrpcpp::InternalErrorException("Stream not created", request->id());
                stream->start(); // We start the stream, otherwise it would be silent
                // Setup response
                result["id"] = stream->getId();
            }
            else if (request->method() == "Stream.RemoveStream")
            {
                // clang-format off
                // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.RemoveStream","params":{"id":"Spotify"}}
                // Response:     {"id":4,"jsonrpc":"2.0","result":{"id":"Spotify"}}
                // clang-format on

                LOG(INFO, LOG_TAG) << "Stream.RemoveStream(" << request->params().get("id") << ")"
                                   << "\n";

                // Find stream
                string streamId = request->params().get("id");
                streamManager_->removeStream(streamId);
                // Setup response
                result["id"] = streamId;
            }
            else
                throw jsonrpcpp::MethodNotFoundException(request->id());
        }
        else
            throw jsonrpcpp::MethodNotFoundException(request->id());

        response = std::make_shared<jsonrpcpp::Response>(*request, result);
    }
    catch (const jsonrpcpp::RequestException& e)
    {
        LOG(ERROR, LOG_TAG) << "Server::onMessageReceived JsonRequestException: " << e.to_json().dump() << ", message: " << request->to_json().dump() << "\n";
        response = std::make_shared<jsonrpcpp::RequestException>(e);
    }
    catch (const exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Server::onMessageReceived exception: " << e.what() << ", message: " << request->to_json().dump() << "\n";
        response = std::make_shared<jsonrpcpp::InternalErrorException>(e.what(), request->id());
    }
    on_response(std::move(response), std::move(notification));
}


void Server::onMessageReceived(std::shared_ptr<ControlSession> controlSession, const std::string& message, const ResponseHander& response_handler)
{
    // LOG(DEBUG, LOG_TAG) << "onMessageReceived: " << message << "\n";
    std::lock_guard<std::mutex> lock(Config::instance().getMutex());
    jsonrpcpp::entity_ptr entity(nullptr);
    try
    {
        entity = jsonrpcpp::Parser::do_parse(message);
        if (!entity)
            return response_handler("");
    }
    catch (const jsonrpcpp::ParseErrorException& e)
    {
        return response_handler(e.to_json().dump());
    }
    catch (const std::exception& e)
    {
        return response_handler(jsonrpcpp::ParseErrorException(e.what()).to_json().dump());
    }

    jsonrpcpp::entity_ptr response(nullptr);
    jsonrpcpp::notification_ptr notification(nullptr);
    if (entity->is_request())
    {
        jsonrpcpp::request_ptr request = dynamic_pointer_cast<jsonrpcpp::Request>(entity);
        processRequest(request,
                       [this, controlSession, response_handler](jsonrpcpp::entity_ptr response, jsonrpcpp::notification_ptr notification)
                       {
            saveConfig();
            ////cout << "Request:      " << request->to_json().dump() << "\n";
            if (notification)
            {
                ////cout << "Notification: " << notification->to_json().dump() << "\n";
                controlServer_->send(notification->to_json().dump(), controlSession.get());
            }
            if (response)
            {
                ////cout << "Response:     " << response->to_json().dump() << "\n";
                return response_handler(response->to_json().dump());
            }
            return response_handler("");
        });
    }
    else if (entity->is_batch())
    {
        /// Attention: this will only work as long as the response handler in processRequest is called synchronously. One way to do this is to remove the outer
        /// loop and to call the next processRequest with
        /// This is true for volume changes, which is the only batch request, but not for Control commands!
        jsonrpcpp::batch_ptr batch = dynamic_pointer_cast<jsonrpcpp::Batch>(entity);
        ////cout << "Batch: " << batch->to_json().dump() << "\n";
        jsonrpcpp::Batch responseBatch;
        jsonrpcpp::Batch notificationBatch;
        for (const auto& batch_entity : batch->entities)
        {
            if (batch_entity->is_request())
            {
                jsonrpcpp::request_ptr request = dynamic_pointer_cast<jsonrpcpp::Request>(batch_entity);
                processRequest(request,
                               [controlSession, response_handler, &responseBatch, &notificationBatch](jsonrpcpp::entity_ptr response,
                                                                                                      jsonrpcpp::notification_ptr notification)
                               {
                    if (response != nullptr)
                        responseBatch.add_ptr(response);
                    if (notification != nullptr)
                        notificationBatch.add_ptr(notification);
                });
            }
        }
        saveConfig();
        if (!notificationBatch.entities.empty())
            controlServer_->send(notificationBatch.to_json().dump(), controlSession.get());
        if (!responseBatch.entities.empty())
            return response_handler(responseBatch.to_json().dump());
        return response_handler("");
    }
    return response_handler("");
}



void Server::onMessageReceived(StreamSession* streamSession, const msg::BaseMessage& baseMessage, char* buffer)
{
    LOG(DEBUG, LOG_TAG) << "onMessageReceived: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id
                        << ", refers: " << baseMessage.refersTo << ", sent: " << baseMessage.sent.sec << "," << baseMessage.sent.usec
                        << ", recv: " << baseMessage.received.sec << "," << baseMessage.received.usec << "\n";

    std::lock_guard<std::mutex> lock(Config::instance().getMutex());
    if (baseMessage.type == message_type::kTime)
    {
        auto timeMsg = make_shared<msg::Time>();
        timeMsg->deserialize(baseMessage, buffer);
        timeMsg->refersTo = timeMsg->id;
        timeMsg->latency = timeMsg->received - timeMsg->sent;
        // LOG(INFO, LOG_TAG) << "Latency sec: " << timeMsg.latency.sec << ", usec: " << timeMsg.latency.usec << ", refers to: " << timeMsg.refersTo <<
        // "\n";
        streamSession->send(timeMsg);

        // refresh streamSession state
        ClientInfoPtr client = Config::instance().getClientInfo(streamSession->clientId);
        if (client != nullptr)
        {
            chronos::systemtimeofday(&client->lastSeen);
            client->connected = true;
        }
    }
    else if (baseMessage.type == message_type::kClientInfo)
    {
        ClientInfoPtr clientInfo = Config::instance().getClientInfo(streamSession->clientId);
        if (clientInfo == nullptr)
        {
            LOG(ERROR, LOG_TAG) << "client not found: " << streamSession->clientId << "\n";
            return;
        }
        msg::ClientInfo infoMsg;
        infoMsg.deserialize(baseMessage, buffer);

        clientInfo->config.volume.percent = infoMsg.getVolume();
        clientInfo->config.volume.muted = infoMsg.isMuted();
        jsonrpcpp::notification_ptr notification = make_shared<jsonrpcpp::Notification>(
            "Client.OnVolumeChanged", jsonrpcpp::Parameter("id", streamSession->clientId, "volume", clientInfo->config.volume.toJson()));
        controlServer_->send(notification->to_json().dump());
    }
    else if (baseMessage.type == message_type::kHello)
    {
        msg::Hello helloMsg;
        helloMsg.deserialize(baseMessage, buffer);
        streamSession->clientId = helloMsg.getUniqueId();
        LOG(INFO, LOG_TAG) << "Hello from " << streamSession->clientId << ", host: " << helloMsg.getHostName() << ", v" << helloMsg.getVersion()
                           << ", ClientName: " << helloMsg.getClientName() << ", OS: " << helloMsg.getOS() << ", Arch: " << helloMsg.getArch()
                           << ", Protocol version: " << helloMsg.getProtocolVersion() << "\n";

        bool newGroup(false);
        GroupPtr group = Config::instance().getGroupFromClient(streamSession->clientId);
        if (group == nullptr)
        {
            group = Config::instance().addClientInfo(streamSession->clientId);
            newGroup = true;
        }

        ClientInfoPtr client = group->getClient(streamSession->clientId);
        if (newGroup)
        {
            client->config.volume.percent = settings_.streamingclient.initialVolume;
        }

        LOG(DEBUG, LOG_TAG) << "Sending ServerSettings to " << streamSession->clientId << "\n";
        auto serverSettings = make_shared<msg::ServerSettings>();
        serverSettings->setVolume(client->config.volume.percent);
        serverSettings->setMuted(client->config.volume.muted || group->muted);
        serverSettings->setLatency(client->config.latency);
        serverSettings->setBufferMs(settings_.stream.bufferMs);
        serverSettings->refersTo = helloMsg.id;
        streamSession->send(serverSettings);

        client->host.mac = helloMsg.getMacAddress();
        client->host.ip = streamSession->getIP();
        client->host.name = helloMsg.getHostName();
        client->host.os = helloMsg.getOS();
        client->host.arch = helloMsg.getArch();
        client->snapclient.version = helloMsg.getVersion();
        client->snapclient.name = helloMsg.getClientName();
        client->snapclient.protocolVersion = helloMsg.getProtocolVersion();
        client->config.instance = helloMsg.getInstance();
        client->connected = true;
        chronos::systemtimeofday(&client->lastSeen);

        // Assign and update stream
        PcmStreamPtr stream = streamManager_->getStream(group->streamId);
        if (!stream)
        {
            stream = streamManager_->getDefaultStream();
            group->streamId = stream->getId();
        }
        LOG(DEBUG, LOG_TAG) << "Group: " << group->id << ", stream: " << group->streamId << "\n";

        saveConfig();

        // LOG(DEBUG, LOG_TAG) << "Sending meta data to " << streamSession->clientId << "\n";
        // streamSession->send(stream->getMeta());
        streamSession->setPcmStream(stream);
        auto headerChunk = stream->getHeader();
        LOG(DEBUG, LOG_TAG) << "Sending codec header to " << streamSession->clientId << "\n";
        streamSession->send(headerChunk);

        if (newGroup)
        {
            // clang-format off
            // Notification: {"jsonrpc":"2.0","method":"Server.OnUpdate","params":{"server":{"groups":[{"clients":[{"config":{"instance":2,"latency":6,"name":"123 456","volume":{"muted":false,"percent":48}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc#2","lastSeen":{"sec":1488025796,"usec":714671},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"4dcc4e3b-c699-a04b-7f0c-8260d23c43e1","muted":false,"name":"","stream_id":"stream 2"},{"clients":[{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":100}},"connected":true,"host":{"arch":"x86_64","ip":"127.0.0.1","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025798,"usec":728305},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}}],"id":"c5da8f7a-f377-1e51-8266-c5cc61099b71","muted":false,"name":"","stream_id":"stream 1"}],"server":{"host":{"arch":"x86_64","ip":"","mac":"","name":"T400","os":"Linux Mint 17.3 Rosa"},"snapserver":{"controlProtocolVersion":1,"name":"Snapserver","protocolVersion":1,"version":"0.10.0"}},"streams":[{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}},{"id":"stream 2","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 2","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 2","scheme":"pipe"}}]}}}
            // clang-format on
            json server = Config::instance().getServerStatus(streamManager_->toJson());
            json notification = jsonrpcpp::Notification("Server.OnUpdate", jsonrpcpp::Parameter("server", server)).to_json();
            controlServer_->send(notification.dump());
        }
        else
        {
            // clang-format off
            // Notification: {"jsonrpc":"2.0","method":"Client.OnConnect","params":{"client":{"config":{"instance":1,"latency":0,"name":"","volume":{"muted":false,"percent":81}},"connected":true,"host":{"arch":"x86_64","ip":"192.168.0.54","mac":"00:21:6a:7d:74:fc","name":"T400","os":"Linux Mint 17.3 Rosa"},"id":"00:21:6a:7d:74:fc","lastSeen":{"sec":1488025524,"usec":876332},"snapclient":{"name":"Snapclient","protocolVersion":2,"version":"0.10.0"}},"id":"00:21:6a:7d:74:fc"}}
            // clang-format on
            json notification = jsonrpcpp::Notification("Client.OnConnect", jsonrpcpp::Parameter("id", client->id, "client", client->toJson())).to_json();
            controlServer_->send(notification.dump());
            // cout << "Notification: " << notification.dump() << "\n";
        }
        //		cout << Config::instance().getServerStatus(streamManager_->toJson()).dump(4) << "\n";
        //		cout << group->toJson().dump(4) << "\n";
    }
}


void Server::saveConfig(const std::chrono::milliseconds& deferred)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    config_timer_.cancel();
    config_timer_.expires_after(deferred);
    config_timer_.async_wait(
        [](const boost::system::error_code& ec)
        {
        if (!ec)
        {
            LOG(DEBUG, LOG_TAG) << "Saving config\n";
            Config::instance().save();
        }
    });
}


void Server::start()
{
    try
    {
        controlServer_ = std::make_unique<ControlServer>(io_context_, settings_.tcp, settings_.http, this);
        streamServer_ = std::make_unique<StreamServer>(io_context_, settings_, this);
        streamManager_ = std::make_unique<StreamManager>(this, io_context_, settings_);

        // Add normal sources first
        for (const auto& sourceUri : settings_.stream.sources)
        {
            StreamUri streamUri(sourceUri);
            if (streamUri.scheme == "meta")
                continue;
            PcmStreamPtr stream = streamManager_->addStream(streamUri);
            if (stream)
                LOG(INFO, LOG_TAG) << "Stream: " << stream->getUri().toJson() << "\n";
        }
        // Add meta sources second
        for (const auto& sourceUri : settings_.stream.sources)
        {
            StreamUri streamUri(sourceUri);
            if (streamUri.scheme != "meta")
                continue;
            PcmStreamPtr stream = streamManager_->addStream(streamUri);
            if (stream)
                LOG(INFO, LOG_TAG) << "Stream: " << stream->getUri().toJson() << "\n";
        }

        streamManager_->start();
        controlServer_->start();
        streamServer_->start();
    }
    catch (const std::exception& e)
    {
        LOG(NOTICE, LOG_TAG) << "Server::start: " << e.what() << endl;
        stop();
        throw;
    }
}


void Server::stop()
{
    if (streamManager_)
    {
        streamManager_->stop();
        streamManager_ = nullptr;
    }

    if (controlServer_)
    {
        controlServer_->stop();
        controlServer_ = nullptr;
    }

    if (streamServer_)
    {
        streamServer_->stop();
        streamServer_ = nullptr;
    }
}
