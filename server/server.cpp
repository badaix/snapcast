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
#include "server.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/message/client_info.hpp"
#include "common/message/hello.hpp"
#include "common/message/server_settings.hpp"
#include "common/message/time.hpp"
#include "config.hpp"
#include "jsonrpcpp.hpp"

// 3rd party headers

// standard headers
#include <chrono>
#include <iostream>
#include <memory>


using namespace std;
using namespace streamreader;

using json = nlohmann::json;

static constexpr auto LOG_TAG = "Server";



Server::Server(boost::asio::io_context& io_context, const ServerSettings& serverSettings)
    : io_context_(io_context), config_timer_(io_context), settings_(serverSettings), request_factory_(*this)
{
}


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


void Server::processRequest(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const Request::OnResponse& on_response) const
{
    auto req = request_factory_.getRequest(request->method());
    if (req)
    {
        try
        {
            req->execute(request, authinfo, on_response);
        }
        catch (const jsonrpcpp::RequestException& e)
        {
            LOG(ERROR, LOG_TAG) << "Server::onMessageReceived JsonRequestException: " << e.to_json().dump() << ", message: " << request->to_json().dump()
                                << "\n";
            auto response = std::make_shared<jsonrpcpp::RequestException>(e);
            on_response(std::move(response), nullptr);
        }
        catch (const exception& e)
        {
            LOG(ERROR, LOG_TAG) << "Server::onMessageReceived exception: " << e.what() << ", message: " << request->to_json().dump() << "\n";
            auto response = std::make_shared<jsonrpcpp::InternalErrorException>(e.what(), request->id());
            on_response(std::move(response), nullptr);
        }
    }
    else
    {
        LOG(ERROR, LOG_TAG) << "Method not found: " << request->method() << "\n";
        auto response = std::make_shared<jsonrpcpp::MethodNotFoundException>(request->id());
        on_response(std::move(response), nullptr);
    }
}


void Server::onMessageReceived(std::shared_ptr<ControlSession> controlSession, const std::string& message, const ResponseHandler& response_handler)
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
        processRequest(request, controlSession->authinfo,
                       [this, controlSession, response_handler](jsonrpcpp::entity_ptr response, jsonrpcpp::notification_ptr notification)
        {
            // if (controlSession->authinfo.hasAuthInfo())
            // {
            //     LOG(INFO, LOG_TAG) << "Request auth info - username: " << controlSession->authinfo->username()
            //                        << ", valid: " << controlSession->authinfo->valid() << "\n";
            // }
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
                processRequest(request, controlSession->authinfo,
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
    config_timer_.async_wait([](const boost::system::error_code& ec)
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
        controlServer_ = std::make_unique<ControlServer>(io_context_, settings_, this);
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
        LOG(NOTICE, LOG_TAG) << "Server::start: " << e.what() << "\n";
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
