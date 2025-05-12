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
#include "authinfo.hpp"
#include "config.hpp"
#include "jsonrpcpp.hpp"
#include "stream_server.hpp"
#include "streamreader/stream_manager.hpp"

// 3rd party headers

// standard headers
#include <functional>
#include <string>
#include <vector>

class Server;

/// Base class of a Snapserver control request
class Request
{
public:
    /// Description of the request
    struct Description
    {
        /// Type of a value or parameter
        enum class Type
        {
            string,  ///< string
            number,  ///< integer
            boolean, ///< bool
            array,   ///< array
            object,  ///< json object
            null,    ///< null
        };

        /// @return the string representation of @p type 
        std::string toString(Type type)
        {
            switch (type)
            {
                case Type::string:
                    return "string";
                case Type::number:
                    return "number";
                case Type::boolean:
                    return "boolean";
                case Type::array:
                    return "array";
                case Type::object:
                    return "object";
                case Type::null:
                default:
                    return "null";
            }
        };

        /// An RPC parameter
        struct Parameter
        {
            std::string name;        ///< param name
            Type type;               ///< param type (string, number, bool, array, object)
            std::string description; ///< param description
        };

        /// The RPC command's resilt
        struct Result
        {
            Type type = Type::null;  ///< result type (string, number, bool, array, object)
            std::string description; ///< result description
        };

        /// c'tor
        // NOLINTNEXTLINE
        Description(std::string description, std::vector<Parameter> parameters = {}, Result result = {Type::null, ""})
            : description(std::move(description)), parameters(std::move(parameters)), result(std::move(result))
        {
        }

        /// Description
        std::string description;
        /// Parameters
        std::vector<Parameter> parameters;
        /// Return value
        Result result;

        /// @return description as json
        Json toJson()
        {
            Json jres;
            jres["description"] = description;
            if (result.type != Type::null)
                jres["return"] = {toString(result.type), result.description};
            if (!parameters.empty())
            {
                Json jparams = Json::array();
                for (const auto& parameter : parameters)
                {
                    Json jparam;
                    jparam["parameter"] = parameter.name;
                    jparam["description"] = parameter.description;
                    jparam["type"] = toString(parameter.type);
                    jparams.push_back(std::move(jparam));
                }
                jres["parameters"] = jparams;
            }
            return jres;
        }
    };

    // TODO: revise handler names
    /// Response handler for json control requests, returning a @p response and/or a @p notification broadcast
    using OnResponse = std::function<void(jsonrpcpp::entity_ptr response, jsonrpcpp::notification_ptr notification)>;

    /// No default c'tor
    Request() = delete;

    /// c'tor
    explicit Request(const Server& server, std::string method);

    /// d'tor
    virtual ~Request() = default;

    /// Execute the Request
    virtual void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) = 0;

    /// @return description
    virtual Description description() const = 0;

    /// @return true if the user has the permission for the request
    virtual bool hasPermission(const AuthInfo& authinfo) const;

    /// @return user needs to be authenticated
    virtual bool requiresAuthentication() const;
    /// @return user needs authorization
    virtual bool requiresAuthorization() const;

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
    Description description() const override;
};


/// Base for "Client.SetVolume" requests
class ClientSetVolumeRequest : public ClientRequest
{
public:
    /// c'tor
    explicit ClientSetVolumeRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// Base for "Client.SetLatency" requests
class ClientSetLatencyRequest : public ClientRequest
{
public:
    /// c'tor
    explicit ClientSetLatencyRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// Base for "Client.SetName" requests
class ClientSetNameRequest : public ClientRequest
{
public:
    /// c'tor
    explicit ClientSetNameRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
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
    Description description() const override;
};


/// "Group.SetName" request
class GroupSetNameRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupSetNameRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// "Group.SetMute" request
class GroupSetMuteRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupSetMuteRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// "Group.SetStream" request
class GroupSetStreamRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupSetStreamRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// "Group.SetClients" request
class GroupSetClientsRequest : public GroupRequest
{
public:
    /// c'tor
    explicit GroupSetClientsRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
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
    Description description() const override;
};


/// "Stream.SetProperty" request
class StreamSetPropertyRequest : public StreamRequest
{
public:
    /// c'tor
    explicit StreamSetPropertyRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// "Stream.AddStream" request
class StreamAddRequest : public StreamRequest
{
public:
    /// c'tor
    explicit StreamAddRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// "Stream.RemoveStream" request
class StreamRemoveRequest : public StreamRequest
{
public:
    /// c'tor
    explicit StreamRemoveRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};



/// "Server.GetRPCVersion" request
class ServerGetRpcVersionRequest : public Request
{
public:
    /// c'tor
    explicit ServerGetRpcVersionRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// "Server.GetStatus" request
class ServerGetStatusRequest : public Request
{
public:
    /// c'tor
    explicit ServerGetStatusRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// "Server.DeleteClient" request
class ServerDeleteClientRequest : public Request
{
public:
    /// c'tor
    explicit ServerDeleteClientRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;
};


/// "Server.Authenticate" request
class ServerAuthenticateRequest : public Request
{
public:
    /// c'tor
    explicit ServerAuthenticateRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;

    bool requiresAuthentication() const override
    {
        return false;
    }

    bool requiresAuthorization() const override
    {
        return false;
    }
};


#if 0
/// "Server.GetToken" request
class ServerGetTokenRequest : public Request
{
public:
    /// c'tor
    explicit ServerGetTokenRequest(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
};
#endif


/// "General.GetRpcCommands" request
class GeneralGetRpcCommands : public Request
{
public:
    /// c'tor
    explicit GeneralGetRpcCommands(const Server& server);
    void execute(const jsonrpcpp::request_ptr& request, AuthInfo& authinfo, const OnResponse& on_response) override;
    Description description() const override;

    /// Set available @p requests
    void setCommands(std::vector<std::shared_ptr<Request>> requests);

    bool requiresAuthentication() const override
    {
        return true;
    }

    bool requiresAuthorization() const override
    {
        return false;
    }

private:
    std::vector<std::shared_ptr<Request>> requests_;
};
