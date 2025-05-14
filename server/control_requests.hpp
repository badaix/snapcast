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
#include "authinfo.hpp"
#include "config.hpp"
#include "jsonrpcpp.hpp"
#include "stream_server.hpp"
#include "streamreader/stream_manager.hpp"

// 3rd party headers

// standard headers
#include <functional>
#include <string>

class Server;

/// Base class of a Snapserver control request
class Request
{
public:
    // TODO: revise handler names
    /// Response handler for json control requests, returning a @p response and/or a @p notification broadcast
    using OnResponse = std::function<void(jsonrpcpp::entity_ptr response, jsonrpcpp::notification_ptr notification)>;

    /// No default c'tor
    Request() = delete;

    /// c'tor
    explicit Request(const Server& server, const std::string& method);

    /// d'tor
    virtual ~Request() = default;

    /// Execute the Request
    virtual void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) = 0;

    /// @return true if the user has the permission for the request
    bool hasPermission(const AuthInfo& authinfo) const;

    /// @return the name of the method
    const std::string& method() const;

protected:
    /// @return the server's stream server
    const StreamServer& getStreamServer() const;
    /// @return the server's stream manager
    StreamManager& getStreamManager() const;
    /// @return server settings
    const ServerSettings& getSettings() const;


private:
    /// the server
    const Server& server_;
    /// the command
    std::string method_;
    /// the ressource
    std::string ressource_;
};


/// Control request factory
class ControlRequestFactory
{
public:
    /// c'tor
    explicit ControlRequestFactory(const Server& server);
    /// @return Request instance to handle @p method
    std::shared_ptr<Request> getRequest(const std::string& method) const;

private:
    /// storage for all available requests
    std::map<std::string, std::shared_ptr<Request>> request_map_;
};



/// Base for "Client." requests
class ClientRequest : public Request
{
public:
    /// c'tor
    ClientRequest(const Server& server, const std::string& method);

protected:
    /// update the client that is referenced in the @p request
    void updateClient(const jsonrpcpp::request_ptr& request);

    /// @return the client referenced in the request
    static ClientInfoPtr getClient(const jsonrpcpp::request_ptr& request);
};


/// Base for "Client.GetStatus" requests
class ClientGetStatusRequest : public ClientRequest
{
public:
    /// c'tor
    explicit ClientGetStatusRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// Base for "Client.SetVolume" requests
class ClientSetVolumeRequest : public ClientRequest
{
public:
    /// c'tor
    explicit ClientSetVolumeRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// Base for "Client.SetLatency" requests
class ClientSetLatencyRequest : public ClientRequest
{
public:
    /// c'tor
    explicit ClientSetLatencyRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// Base for "Client.SetName" requests
class ClientSetNameRequest : public ClientRequest
{
public:
    /// c'tor
    explicit ClientSetNameRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};



/// Base for "Group." requests
class GroupRequest : public Request
{
public:
    /// c'tor
    GroupRequest(const Server& server, const std::string& method);

protected:
    /// @return the group referenced in the request
    static GroupPtr getGroup(const jsonrpcpp::request_ptr& request);
};


/// "Group.GetStatus" request
class GroupGetStatusRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupGetStatusRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Group.SetName" request
class GroupSetNameRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupSetNameRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Group.SetMute" request
class GroupSetMuteRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupSetMuteRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Group.SetStream" request
class GroupSetStreamRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupSetStreamRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Group.SetClients" request
class GroupSetClientsRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupSetClientsRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};



/// Base for "Stream." requests
class StreamRequest : public Request
{
public:
    /// c'tor
    StreamRequest(const Server& server, const std::string& method);

protected:
    /// @return the stream referenced in the request
    static PcmStreamPtr getStream(const StreamManager& stream_manager, const jsonrpcpp::request_ptr& request);
    /// @return the stream id referenced in the request
    static std::string getStreamId(const jsonrpcpp::request_ptr& request);
};


/// "Stream.Control" request
class StreamControlRequest : public StreamRequest
{
public:
    /// c'tor
    explicit StreamControlRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Stream.SetProperty" request
class StreamSetPropertyRequest : public StreamRequest
{
public:
    /// c'tor
    explicit StreamSetPropertyRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Stream.AddStream" request
class StreamAddRequest : public StreamRequest
{
public:
    /// c'tor
    explicit StreamAddRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Stream.RemoveStream" request
class StreamRemoveRequest : public StreamRequest
{
public:
    /// c'tor
    explicit StreamRemoveRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};



/// "Server.GetRPCVersion" request
class ServerGetRpcVersionRequest : public Request
{
public:
    /// c'tor
    explicit ServerGetRpcVersionRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Server.GetStatus" request
class ServerGetStatusRequest : public Request
{
public:
    /// c'tor
    explicit ServerGetStatusRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Server.DeleteClient" request
class ServerDeleteClientRequest : public Request
{
public:
    /// c'tor
    explicit ServerDeleteClientRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Server.Authenticate" request
class ServerAuthenticateRequest : public Request
{
public:
    /// c'tor
    explicit ServerAuthenticateRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};


/// "Server.GetToken" request
class ServerGetTokenRequest : public Request
{
public:
    /// c'tor
    explicit ServerGetTokenRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};
