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

#include "stream_server.hpp"
#include "common/aixlog.hpp"
#include "config.hpp"
#include "message/hello.hpp"
#include "message/stream_tags.hpp"
#include "message/time.hpp"
#include <iostream>

using namespace std;
using namespace streamreader;

using json = nlohmann::json;


StreamServer::StreamServer(boost::asio::io_context& io_context, const ServerSettings& serverSettings) : io_context_(io_context), settings_(serverSettings)
{
}


StreamServer::~StreamServer() = default;


void StreamServer::cleanup()
{
    auto new_end = std::remove_if(sessions_.begin(), sessions_.end(), [](std::weak_ptr<StreamSession> session) { return session.expired(); });
    auto count = distance(new_end, sessions_.end());
    if (count > 0)
    {
        SLOG(ERROR) << "Removing " << count << " inactive session(s), active sessions: " << sessions_.size() - count << "\n";
        sessions_.erase(new_end, sessions_.end());
    }
}


void StreamServer::onMetaChanged(const PcmStream* pcmStream)
{
    // clang-format off
    // Notification: {"jsonrpc":"2.0","method":"Stream.OnMetadata","params":{"id":"stream 1", "meta": {"album": "some album", "artist": "some artist", "track": "some track"...}}
    // clang-format on

    // Send meta to all connected clients
    const auto meta = pcmStream->getMeta();
    LOG(DEBUG) << "metadata = " << meta->msg.dump(3) << "\n";

    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    for (auto s : sessions_)
    {
        if (auto session = s.lock())
        {
            if (session->pcmStream().get() == pcmStream)
                session->sendAsync(meta);
        }
    }

    LOG(INFO) << "onMetaChanged (" << pcmStream->getName() << ")\n";
    json notification = jsonrpcpp::Notification("Stream.OnMetadata", jsonrpcpp::Parameter("id", pcmStream->getId(), "meta", meta->msg)).to_json();
    controlServer_->send(notification.dump(), nullptr);
    // cout << "Notification: " << notification.dump() << "\n";
}

void StreamServer::onStateChanged(const PcmStream* pcmStream, const ReaderState& state)
{
    // clang-format off
    // Notification: {"jsonrpc":"2.0","method":"Stream.OnUpdate","params":{"id":"stream 1","stream":{"id":"stream 1","status":"idle","uri":{"fragment":"","host":"","path":"/tmp/snapfifo","query":{"chunk_ms":"20","codec":"flac","name":"stream 1","sampleformat":"48000:16:2"},"raw":"pipe:///tmp/snapfifo?name=stream 1","scheme":"pipe"}}}}
    // clang-format on
    LOG(INFO) << "onStateChanged (" << pcmStream->getName() << "): " << static_cast<int>(state) << "\n";
    //	LOG(INFO) << pcmStream->toJson().dump(4);
    json notification = jsonrpcpp::Notification("Stream.OnUpdate", jsonrpcpp::Parameter("id", pcmStream->getId(), "stream", pcmStream->toJson())).to_json();
    controlServer_->send(notification.dump(), nullptr);
    // cout << "Notification: " << notification.dump() << "\n";
}


void StreamServer::onChunkRead(const PcmStream* pcmStream, msg::PcmChunk* chunk, double /*duration*/)
{
    //	LOG(INFO) << "onChunkRead (" << pcmStream->getName() << "): " << duration << "ms\n";
    bool isDefaultStream(pcmStream == streamManager_->getDefaultStream().get());
    unique_ptr<msg::PcmChunk> chunk_ptr(chunk);

    std::ostringstream oss;
    tv t;
    chunk_ptr->sent = t;
    chunk_ptr->serialize(oss);
    shared_const_buffer buffer(oss.str());

    std::vector<std::shared_ptr<StreamSession>> sessions;
    {
        std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
        for (auto session : sessions_)
            if (auto s = session.lock())
                sessions.push_back(s);
    }

    for (auto session : sessions)
    {
        if (!settings_.stream.sendAudioToMutedClients)
        {
            GroupPtr group = Config::instance().getGroupFromClient(session->clientId);
            if (group)
            {
                if (group->muted)
                {
                    continue;
                }
                else
                {
                    std::lock_guard<std::recursive_mutex> lock(clientMutex_);
                    ClientInfoPtr client = group->getClient(session->clientId);
                    if (client && client->config.volume.muted)
                        continue;
                }
            }
        }

        if (!session->pcmStream() && isDefaultStream) //->getName() == "default")
            session->sendAsync(buffer);
        else if (session->pcmStream().get() == pcmStream)
            session->sendAsync(buffer);
    }
}


void StreamServer::onResync(const PcmStream* pcmStream, double ms)
{
    LOG(INFO) << "onResync (" << pcmStream->getName() << "): " << ms << "ms\n";
}


void StreamServer::onDisconnect(StreamSession* streamSession)
{
    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    session_ptr session = getStreamSession(streamSession);

    if (session == nullptr)
        return;

    LOG(INFO) << "onDisconnect: " << session->clientId << "\n";
    LOG(DEBUG) << "sessions: " << sessions_.size() << "\n";
    sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                   [streamSession](std::weak_ptr<StreamSession> session) {
                                       auto s = session.lock();
                                       return s.get() == streamSession;
                                   }),
                    sessions_.end());
    LOG(DEBUG) << "sessions: " << sessions_.size() << "\n";

    // notify controllers if not yet done
    ClientInfoPtr clientInfo = Config::instance().getClientInfo(session->clientId);
    if (!clientInfo || !clientInfo->connected)
        return;

    clientInfo->connected = false;
    chronos::systemtimeofday(&clientInfo->lastSeen);
    Config::instance().save();
    if (controlServer_ != nullptr)
    {
        // Check if there is no session of this client is left
        // Can happen in case of ungraceful disconnect/reconnect or
        // in case of a duplicate client id
        if (getStreamSession(clientInfo->id) == nullptr)
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
    cleanup();
}


void StreamServer::ProcessRequest(const jsonrpcpp::request_ptr request, jsonrpcpp::entity_ptr& response, jsonrpcpp::notification_ptr& notification) const
{
    try
    {
        // LOG(INFO) << "StreamServer::ProcessRequest method: " << request->method << ", " << "id: " << request->id() << "\n";
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

                std::lock_guard<std::recursive_mutex> lock(clientMutex_);
                clientInfo->config.volume.fromJson(request->params().get("volume"));
                result["volume"] = clientInfo->config.volume.toJson();
                notification.reset(new jsonrpcpp::Notification("Client.OnVolumeChanged",
                                                               jsonrpcpp::Parameter("id", clientInfo->id, "volume", clientInfo->config.volume.toJson())));
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
                notification.reset(
                    new jsonrpcpp::Notification("Client.OnLatencyChanged", jsonrpcpp::Parameter("id", clientInfo->id, "latency", clientInfo->config.latency)));
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
                notification.reset(
                    new jsonrpcpp::Notification("Client.OnNameChanged", jsonrpcpp::Parameter("id", clientInfo->id, "name", clientInfo->config.name)));
            }
            else
                throw jsonrpcpp::MethodNotFoundException(request->id());


            if (request->method().find("Client.Set") == 0)
            {
                /// Update client
                session_ptr session = getStreamSession(clientInfo->id);
                if (session != nullptr)
                {
                    auto serverSettings = make_shared<msg::ServerSettings>();
                    serverSettings->setBufferMs(settings_.stream.bufferMs);
                    serverSettings->setVolume(clientInfo->config.volume.percent);
                    GroupPtr group = Config::instance().getGroupFromClient(clientInfo);
                    serverSettings->setMuted(clientInfo->config.volume.muted || group->muted);
                    serverSettings->setLatency(clientInfo->config.latency);
                    session->sendAsync(serverSettings);
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
                notification.reset(new jsonrpcpp::Notification("Group.OnNameChanged", jsonrpcpp::Parameter("id", group->id, "name", group->name)));
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
                for (auto client : group->clients)
                {
                    session_ptr session = getStreamSession(client->id);
                    if (session != nullptr)
                    {
                        auto serverSettings = make_shared<msg::ServerSettings>();
                        serverSettings->setBufferMs(settings_.stream.bufferMs);
                        serverSettings->setVolume(client->config.volume.percent);
                        GroupPtr group = Config::instance().getGroupFromClient(client);
                        serverSettings->setMuted(client->config.volume.muted || group->muted);
                        serverSettings->setLatency(client->config.latency);
                        session->sendAsync(serverSettings);
                    }
                }

                result["mute"] = group->muted;
                notification.reset(new jsonrpcpp::Notification("Group.OnMute", jsonrpcpp::Parameter("id", group->id, "mute", group->muted)));
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
                for (auto client : group->clients)
                {
                    session_ptr session = getStreamSession(client->id);
                    if (session && (session->pcmStream() != stream))
                    {
                        session->sendAsync(stream->getMeta());
                        session->sendAsync(stream->getHeader());
                        session->setPcmStream(stream);
                    }
                }

                // Notify others
                result["stream_id"] = group->streamId;
                notification.reset(new jsonrpcpp::Notification("Group.OnStreamChanged", jsonrpcpp::Parameter("id", group->id, "stream_id", group->streamId)));
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
                    session_ptr session = getStreamSession(client->id);
                    if (session && stream && (session->pcmStream() != stream))
                    {
                        session->sendAsync(stream->getMeta());
                        session->sendAsync(stream->getHeader());
                        session->setPcmStream(stream);
                    }
                }

                if (group->empty())
                    Config::instance().remove(group);

                json server = Config::instance().getServerStatus(streamManager_->toJson());
                result["server"] = server;

                // Notify others: since at least two groups are affected, send a complete server update
                notification.reset(new jsonrpcpp::Notification("Server.OnUpdate", jsonrpcpp::Parameter("server", server)));
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
                notification.reset(new jsonrpcpp::Notification("Server.OnUpdate", jsonrpcpp::Parameter("server", server)));
            }
            else
                throw jsonrpcpp::MethodNotFoundException(request->id());
        }
        else if (request->method().find("Stream.") == 0)
        {
            if (request->method().find("Stream.SetMeta") == 0)
            {
                /// Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.SetMeta","params":{"id":"Spotify",
                ///                "meta": {"album": "some album", "artist": "some artist", "track": "some track"...}}}
                ///
                /// Response:     {"id":4,"jsonrpc":"2.0","result":{"stream_id":"Spotify"}}
                /// Call onMetaChanged(const PcmStream* pcmStream) for updates and notifications

                LOG(INFO) << "Stream.SetMeta(" << request->params().get<std::string>("id") << ")" << request->params().get("meta") << "\n";

                // Find stream
                string streamId = request->params().get<std::string>("id");
                PcmStreamPtr stream = streamManager_->getStream(streamId);
                if (stream == nullptr)
                    throw jsonrpcpp::InternalErrorException("Stream not found", request->id());

                // Set metadata from request
                stream->setMeta(request->params().get("meta"));

                // Setup response
                result["id"] = streamId;
            }
            else if (request->method() == "Stream.AddStream")
            {
                // clang-format off
                // Request:      {"id":4,"jsonrpc":"2.0","method":"Stream.AddStream","params":{"streamUri":"uri"}}
                // Response:     {"id":4,"jsonrpc":"2.0","result":{"stream_id":"Spotify"}}
                // Call onMetaChanged(const PcmStream* pcmStream) for updates and notifications
                // clang-format on

                LOG(INFO) << "Stream.AddStream(" << request->params().get("streamUri") << ")"
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
                // Response:     {"id":4,"jsonrpc":"2.0","result":{"stream_id":"Spotify"}}
                // Call onMetaChanged(const PcmStream* pcmStream) for updates and notifications
                // clang-format on

                LOG(INFO) << "Stream.RemoveStream(" << request->params().get("id") << ")"
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

        response.reset(new jsonrpcpp::Response(*request, result));
    }
    catch (const jsonrpcpp::RequestException& e)
    {
        LOG(ERROR) << "StreamServer::onMessageReceived JsonRequestException: " << e.to_json().dump() << ", message: " << request->to_json().dump() << "\n";
        response.reset(new jsonrpcpp::RequestException(e));
    }
    catch (const exception& e)
    {
        LOG(ERROR) << "StreamServer::onMessageReceived exception: " << e.what() << ", message: " << request->to_json().dump() << "\n";
        response.reset(new jsonrpcpp::InternalErrorException(e.what(), request->id()));
    }
}


std::string StreamServer::onMessageReceived(ControlSession* controlSession, const std::string& message)
{
    // LOG(DEBUG) << "onMessageReceived: " << message << "\n";
    jsonrpcpp::entity_ptr entity(nullptr);
    try
    {
        entity = jsonrpcpp::Parser::do_parse(message);
        if (!entity)
            return "";
    }
    catch (const jsonrpcpp::ParseErrorException& e)
    {
        return e.to_json().dump();
    }
    catch (const std::exception& e)
    {
        return jsonrpcpp::ParseErrorException(e.what()).to_json().dump();
    }

    jsonrpcpp::entity_ptr response(nullptr);
    jsonrpcpp::notification_ptr notification(nullptr);
    if (entity->is_request())
    {
        jsonrpcpp::request_ptr request = dynamic_pointer_cast<jsonrpcpp::Request>(entity);
        ProcessRequest(request, response, notification);
        Config::instance().save();
        ////cout << "Request:      " << request->to_json().dump() << "\n";
        if (notification)
        {
            ////cout << "Notification: " << notification->to_json().dump() << "\n";
            controlServer_->send(notification->to_json().dump(), controlSession);
        }
        if (response)
        {
            ////cout << "Response:     " << response->to_json().dump() << "\n";
            return response->to_json().dump();
        }
        return "";
    }
    else if (entity->is_batch())
    {
        jsonrpcpp::batch_ptr batch = dynamic_pointer_cast<jsonrpcpp::Batch>(entity);
        ////cout << "Batch: " << batch->to_json().dump() << "\n";
        jsonrpcpp::Batch responseBatch;
        jsonrpcpp::Batch notificationBatch;
        for (const auto& batch_entity : batch->entities)
        {
            if (batch_entity->is_request())
            {
                jsonrpcpp::request_ptr request = dynamic_pointer_cast<jsonrpcpp::Request>(batch_entity);
                ProcessRequest(request, response, notification);
                if (response != nullptr)
                    responseBatch.add_ptr(response);
                if (notification != nullptr)
                    notificationBatch.add_ptr(notification);
            }
        }
        Config::instance().save();
        if (!notificationBatch.entities.empty())
            controlServer_->send(notificationBatch.to_json().dump(), controlSession);
        if (!responseBatch.entities.empty())
            return responseBatch.to_json().dump();
        return "";
    }
    return "";
}



void StreamServer::onMessageReceived(StreamSession* streamSession, const msg::BaseMessage& baseMessage, char* buffer)
{
    //	LOG(DEBUG) << "onMessageReceived: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " << baseMessage.id << ", refers: " <<
    // baseMessage.refersTo << ", sent: " << baseMessage.sent.sec << "," << baseMessage.sent.usec << ", recv: " << baseMessage.received.sec << "," <<
    // baseMessage.received.usec << "\n";
    if (baseMessage.type == message_type::kTime)
    {
        auto timeMsg = make_shared<msg::Time>();
        timeMsg->deserialize(baseMessage, buffer);
        timeMsg->refersTo = timeMsg->id;
        timeMsg->latency = timeMsg->received - timeMsg->sent;
        //		LOG(INFO) << "Latency sec: " << timeMsg.latency.sec << ", usec: " << timeMsg.latency.usec << ", refers to: " << timeMsg.refersTo <<
        //"\n";
        streamSession->sendAsync(timeMsg);

        // refresh streamSession state
        ClientInfoPtr client = Config::instance().getClientInfo(streamSession->clientId);
        if (client != nullptr)
        {
            chronos::systemtimeofday(&client->lastSeen);
            client->connected = true;
        }
    }
    else if (baseMessage.type == message_type::kHello)
    {
        msg::Hello helloMsg;
        helloMsg.deserialize(baseMessage, buffer);
        streamSession->clientId = helloMsg.getUniqueId();
        LOG(INFO) << "Hello from " << streamSession->clientId << ", host: " << helloMsg.getHostName() << ", v" << helloMsg.getVersion()
                  << ", ClientName: " << helloMsg.getClientName() << ", OS: " << helloMsg.getOS() << ", Arch: " << helloMsg.getArch()
                  << ", Protocol version: " << helloMsg.getProtocolVersion() << "\n";

        LOG(DEBUG) << "request kServerSettings: " << streamSession->clientId << "\n";
        //		std::lock_guard<std::mutex> mlock(mutex_);
        bool newGroup(false);
        GroupPtr group = Config::instance().getGroupFromClient(streamSession->clientId);
        if (group == nullptr)
        {
            group = Config::instance().addClientInfo(streamSession->clientId);
            newGroup = true;
        }

        ClientInfoPtr client = group->getClient(streamSession->clientId);

        LOG(DEBUG) << "request kServerSettings\n";
        auto serverSettings = make_shared<msg::ServerSettings>();
        serverSettings->setVolume(client->config.volume.percent);
        serverSettings->setMuted(client->config.volume.muted || group->muted);
        serverSettings->setLatency(client->config.latency);
        serverSettings->setBufferMs(settings_.stream.bufferMs);
        serverSettings->refersTo = helloMsg.id;
        streamSession->sendAsync(serverSettings);

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
        LOG(DEBUG) << "Group: " << group->id << ", stream: " << group->streamId << "\n";

        Config::instance().save();

        streamSession->sendAsync(stream->getMeta());
        streamSession->setPcmStream(stream);
        auto headerChunk = stream->getHeader();
        streamSession->sendAsync(headerChunk);

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



session_ptr StreamServer::getStreamSession(StreamSession* streamSession) const
{
    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);

    for (auto session : sessions_)
    {
        if (auto s = session.lock())
            if (s.get() == streamSession)
                return s;
    }
    return nullptr;
}


session_ptr StreamServer::getStreamSession(const std::string& clientId) const
{
    //	LOG(INFO) << "getStreamSession: " << mac << "\n";
    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    for (auto session : sessions_)
    {
        if (auto s = session.lock())
            if (s->clientId == clientId)
                return s;
    }
    return nullptr;
}


void StreamServer::startAccept()
{
    auto accept_handler = [this](error_code ec, tcp::socket socket) {
        if (!ec)
            handleAccept(std::move(socket));
        else
            LOG(ERROR) << "Error while accepting socket connection: " << ec.message() << "\n";
    };

    for (auto& acceptor : acceptor_)
        acceptor->async_accept(accept_handler);
}


void StreamServer::handleAccept(tcp::socket socket)
{
    try
    {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket.native_handle(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        /// experimental: turn on tcp::no_delay
        socket.set_option(tcp::no_delay(true));

        SLOG(NOTICE) << "StreamServer::NewConnection: " << socket.remote_endpoint().address().to_string() << endl;
        shared_ptr<StreamSession> session = make_shared<StreamSession>(io_context_, this, std::move(socket));

        session->setBufferMs(settings_.stream.bufferMs);
        session->start();

        std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
        sessions_.emplace_back(session);
        cleanup();
    }
    catch (const std::exception& e)
    {
        SLOG(ERROR) << "Exception in StreamServer::handleAccept: " << e.what() << endl;
    }
    startAccept();
}


void StreamServer::start()
{
    try
    {
        controlServer_.reset(new ControlServer(io_context_, settings_.tcp, settings_.http, this));
        controlServer_->start();

        streamManager_.reset(new StreamManager(this, io_context_, settings_.stream.sampleFormat, settings_.stream.codec, settings_.stream.streamChunkMs));
        //	throw SnapException("xxx");
        for (const auto& streamUri : settings_.stream.pcmStreams)
        {
            PcmStreamPtr stream = streamManager_->addStream(streamUri);
            if (stream)
                LOG(INFO) << "Stream: " << stream->getUri().toJson() << "\n";
        }
        streamManager_->start();

        for (const auto& address : settings_.stream.bind_to_address)
        {
            try
            {
                LOG(INFO) << "Creating stream acceptor for address: " << address << ", port: " << settings_.stream.port << "\n";
                acceptor_.emplace_back(
                    make_unique<tcp::acceptor>(io_context_, tcp::endpoint(boost::asio::ip::address::from_string(address), settings_.stream.port)));
            }
            catch (const boost::system::system_error& e)
            {
                LOG(ERROR) << "error creating TCP acceptor: " << e.what() << ", code: " << e.code() << "\n";
            }
        }

        startAccept();
    }
    catch (const std::exception& e)
    {
        SLOG(NOTICE) << "StreamServer::start: " << e.what() << endl;
        stop();
        throw;
    }
}


void StreamServer::stop()
{
    for (auto& acceptor : acceptor_)
        acceptor->cancel();
    acceptor_.clear();

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

    std::lock_guard<std::recursive_mutex> mlock(sessionsMutex_);
    cleanup();
    for (auto s : sessions_)
    {
        if (auto session = s.lock())
            session->stop();
    }
}
