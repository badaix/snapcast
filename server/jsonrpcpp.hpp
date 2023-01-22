/***
       __  ____   __   __ _  ____  ____   ___  _    _
     _(  )/ ___) /  \ (  ( \(  _ \(  _ \ / __)( )  ( )
    / \) \\___ \(  O )/    / )   / ) __/( (__(_ _)(_ _)
    \____/(____/ \__/ \_)__)(__\_)(__)   \___)(_)  (_)
    version 1.3.3
    https://github.com/badaix/jsonrpcpp

    This file is part of jsonrpc++
    Copyright (C) 2017-2021 Johannes Pohl

    This software may be modified and distributed under the terms
    of the MIT license.  See the LICENSE file for details.
***/

/// http://patorjk.com/software/taag/#p=display&f=Graceful&t=JSONRPC%2B%2B

/// checked with clang-tidy:
/// run-clang-tidy-3.8.py -header-filter='jsonrpcpp.hpp'
/// -checks='*,-misc-definitions-in-headers,-google-readability-braces-around-statements,-readability-braces-around-statements,-cppcoreguidelines-pro-bounds-pointer-arithmetic,-google-build-using-namespace,-google-build-using-namespace,-modernize-pass-by-value,-google-explicit-constructor'

#ifndef JSON_RPC_HPP
#define JSON_RPC_HPP

#include "common/json.hpp"
#include <cstring>
#include <exception>
#include <string>
#include <vector>


using Json = nlohmann::json;

namespace jsonrpcpp
{

class Entity;
class Request;
class Notification;
class Parameter;
class Response;
class Error;
class Batch;

using entity_ptr = std::shared_ptr<Entity>;
using request_ptr = std::shared_ptr<Request>;
using notification_ptr = std::shared_ptr<Notification>;
using parameter_ptr = std::shared_ptr<Parameter>;
using response_ptr = std::shared_ptr<Response>;
using error_ptr = std::shared_ptr<Error>;
using batch_ptr = std::shared_ptr<Batch>;


class Entity
{
public:
    enum class entity_t : uint8_t
    {
        unknown,
        exception,
        id,
        error,
        response,
        request,
        notification,
        batch
    };

    Entity(entity_t type);
    virtual ~Entity() = default;
    Entity(const Entity&) = default;
    Entity& operator=(const Entity&) = default;

    bool is_exception() const;
    bool is_id() const;
    bool is_error() const;
    bool is_response() const;
    bool is_request() const;
    bool is_notification() const;
    bool is_batch() const;

    virtual std::string type_str() const;

    virtual Json to_json() const = 0;
    virtual void parse_json(const Json& json) = 0;

    virtual void parse(const std::string& json_str);
    virtual void parse(const char* json_str);

protected:
    entity_t entity;
};


class NullableEntity : public Entity
{
public:
    NullableEntity(entity_t type);
    NullableEntity(entity_t type, std::nullptr_t);
#ifdef _MSC_VER
    virtual operator bool() const
#else
    virtual explicit operator bool() const
#endif
    {
        return !isNull;
    }

protected:
    bool isNull;
};


class Id : public Entity
{
public:
    enum class value_t : uint8_t
    {
        null,
        string,
        integer
    };

    Id();
    Id(int id);
    Id(const char* id);
    Id(const std::string& id);
    Id(const Json& json_id);

    Json to_json() const override;
    void parse_json(const Json& json) override;

    friend std::ostream& operator<<(std::ostream& out, const Id& id)
    {
        out << id.to_json();
        return out;
    }

    const value_t& type() const
    {
        return type_;
    }

    int int_id() const
    {
        return int_id_;
    }

    const std::string& string_id() const
    {
        return string_id_;
    }

    bool operator<(const Id& other) const
    {
        return (to_json() < other.to_json());
    }

protected:
    value_t type_;
    int int_id_;
    std::string string_id_;
};


class Parameter : public NullableEntity
{
public:
    enum class value_t : uint8_t
    {
        null,
        array,
        map
    };

    Parameter(std::nullptr_t);
    Parameter(const Json& json = nullptr);
    Parameter(const std::string& key1, const Json& value1, const std::string& key2 = "", const Json& value2 = nullptr, const std::string& key3 = "",
              const Json& value3 = nullptr, const std::string& key4 = "", const Json& value4 = nullptr);

    Json to_json() const override;
    void parse_json(const Json& json) override;

    bool is_array() const;
    bool is_map() const;
    bool is_null() const;

    Json get(const std::string& key) const;
    Json get(size_t idx) const;
    bool has(const std::string& key) const;
    bool has(size_t idx) const;

    void add(const std::string& key, const Json& value);

    template <typename T>
    T get(const std::string& key) const
    {
        return get(key).get<T>();
    }

    template <typename T>
    T get(size_t idx) const
    {
        return get(idx).get<T>();
    }

    template <typename T>
    T get(const std::string& key, const T& default_value) const
    {
        if (!has(key))
            return default_value;
        return get<T>(key);
    }

    template <typename T>
    T get(size_t idx, const T& default_value) const
    {
        if (!has(idx))
            return default_value;
        return get<T>(idx);
    }

    value_t type;
    std::vector<Json> param_array;
    std::map<std::string, Json> param_map;
};


class Error : public NullableEntity
{
public:
    Error(const Json& json = nullptr);
    Error(std::nullptr_t);
    Error(const std::string& message, int code, const Json& data = nullptr);

    Json to_json() const override;
    void parse_json(const Json& json) override;

    int code() const
    {
        return code_;
    }

    const std::string& message() const
    {
        return message_;
    }

    const Json& data() const
    {
        return data_;
    }

protected:
    int code_;
    std::string message_;
    Json data_;
};


/// JSON-RPC 2.0 request
/**
 * Simple jsonrpc 2.0 parser with getters
 * Currently no named parameters are supported, but only array parameters
 */
class Request : public Entity
{
public:
    Request(const Json& json = nullptr);
    Request(const Id& id, const std::string& method, const Parameter& params = nullptr);

    Json to_json() const override;
    void parse_json(const Json& json) override;

    const std::string& method() const
    {
        return method_;
    }

    const Parameter& params() const
    {
        return params_;
    }

    const Id& id() const
    {
        return id_;
    }

protected:
    std::string method_;
    Parameter params_;
    Id id_;
};


class RpcException : public std::exception
{
public:
    RpcException(const char* text);
    RpcException(const std::string& text);

    const char* what() const noexcept override;

protected:
    std::runtime_error m_;
};


class RpcEntityException : public RpcException, public Entity
{
public:
    RpcEntityException(const Error& error);
    RpcEntityException(const std::string& text);
    Json to_json() const override = 0;

    const Error& error() const
    {
        return error_;
    }

protected:
    void parse_json(const Json& json) override;
    Error error_;
};


class ParseErrorException : public RpcEntityException
{
public:
    ParseErrorException(const Error& error);
    ParseErrorException(const std::string& data);
    Json to_json() const override;
};


//	-32600	Invalid Request	The JSON sent is not a valid Request object.
//	-32601	Method not found	The method does not exist / is not available.
//	-32602	Invalid params	Invalid method parameter(s).
//	-32603	Internal error	Internal JSON-RPC error.

class RequestException : public RpcEntityException
{
public:
    RequestException(const Error& error, const Id& requestId = Id());
    Json to_json() const override;

    const Id& id() const
    {
        return id_;
    }

protected:
    Id id_;
};


class InvalidRequestException : public RequestException
{
public:
    InvalidRequestException(const Id& requestId = Id());
    InvalidRequestException(const Request& request);
    InvalidRequestException(const char* data, const Id& requestId = Id());
    InvalidRequestException(const std::string& data, const Id& requestId = Id());
};


class MethodNotFoundException : public RequestException
{
public:
    MethodNotFoundException(const Id& requestId = Id());
    MethodNotFoundException(const Request& request);
    MethodNotFoundException(const char* data, const Id& requestId = Id());
    MethodNotFoundException(const std::string& data, const Id& requestId = Id());
};


class InvalidParamsException : public RequestException
{
public:
    InvalidParamsException(const Id& requestId = Id());
    InvalidParamsException(const Request& request);
    InvalidParamsException(const char* data, const Id& requestId = Id());
    InvalidParamsException(const std::string& data, const Id& requestId = Id());
};


class InternalErrorException : public RequestException
{
public:
    InternalErrorException(const Id& requestId = Id());
    InternalErrorException(const Request& request);
    InternalErrorException(const char* data, const Id& requestId = Id());
    InternalErrorException(const std::string& data, const Id& requestId = Id());
};


class Response : public Entity
{
public:
    Response(const Json& json = nullptr);
    Response(const Id& id, const Json& result);
    Response(const Id& id, const Error& error);
    Response(const Request& request, const Json& result);
    Response(const Request& request, const Error& error);
    Response(const RequestException& exception);

    Json to_json() const override;
    void parse_json(const Json& json) override;

    const Id& id() const
    {
        return id_;
    }

    const Json& result() const
    {
        return result_;
    }

    const Error& error() const
    {
        return error_;
    }

protected:
    Id id_;
    Json result_;
    Error error_;
};


class Notification : public Entity
{
public:
    Notification(const Json& json = nullptr);
    Notification(const char* method, const Parameter& params = nullptr);
    Notification(const std::string& method, const Parameter& params);

    Json to_json() const override;
    void parse_json(const Json& json) override;

    const std::string& method() const
    {
        return method_;
    }

    const Parameter& params() const
    {
        return params_;
    }

protected:
    std::string method_;
    Parameter params_;
};


typedef std::function<void(const Parameter& params)> notification_callback;
typedef std::function<jsonrpcpp::response_ptr(const Id& id, const Parameter& params)> request_callback;

class Parser
{
public:
    Parser() = default;
    virtual ~Parser() = default;

    entity_ptr parse(const std::string& json_str);
    entity_ptr parse_json(const Json& json);

    void register_notification_callback(const std::string& notification, notification_callback callback);
    void register_request_callback(const std::string& request, request_callback callback);

    static entity_ptr do_parse(const std::string& json_str);
    static entity_ptr do_parse_json(const Json& json);
    static bool is_request(const std::string& json_str);
    static bool is_request(const Json& json);
    static bool is_notification(const std::string& json_str);
    static bool is_notification(const Json& json);
    static bool is_response(const std::string& json_str);
    static bool is_response(const Json& json);
    static bool is_batch(const std::string& json_str);
    static bool is_batch(const Json& json);

private:
    std::map<std::string, notification_callback> notification_callbacks_;
    std::map<std::string, request_callback> request_callbacks_;
};


class Batch : public Entity
{
public:
    std::vector<entity_ptr> entities;

    Batch(const Json& json = nullptr);

    Json to_json() const override;
    void parse_json(const Json& json) override;

    template <typename T>
    void add(const T& entity)
    {
        entities.push_back(std::make_shared<T>(entity));
    }

    void add_ptr(const entity_ptr& entity)
    {
        entities.push_back(entity);
    }
};



/////////////////////////// Entity implementation /////////////////////////////

inline Entity::Entity(entity_t type) : entity(type)
{
}

inline bool Entity::is_exception() const
{
    return (entity == entity_t::exception);
}

inline bool Entity::is_id() const
{
    return (entity == entity_t::id);
}

inline bool Entity::is_error() const
{
    return (entity == entity_t::error);
}

inline bool Entity::is_response() const
{
    return (entity == entity_t::response);
}

inline bool Entity::is_request() const
{
    return (entity == entity_t::request);
}

inline bool Entity::is_notification() const
{
    return (entity == entity_t::notification);
}

inline bool Entity::is_batch() const
{
    return (entity == entity_t::batch);
}

inline void Entity::parse(const char* json_str)
{
    // http://www.jsonrpc.org/specification
    //	code	message	meaning
    //	-32700	Parse error	Invalid JSON was received by the server. An error occurred on the server while parsing the JSON text.
    //	-32600	Invalid Request	The JSON sent is not a valid Request object.
    //	-32601	Method not found	The method does not exist / is not available.
    //	-32602	Invalid params	Invalid method parameter(s).
    //	-32603	Internal error	Internal JSON-RPC error.
    //	-32000 to -32099	Server error	Reserved for implementation-defined server-errors.
    try
    {
        parse_json(Json::parse(json_str));
    }
    catch (const RpcException&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw ParseErrorException(e.what());
    }
}

inline void Entity::parse(const std::string& json_str)
{
    parse(json_str.c_str());
}

inline std::string Entity::type_str() const
{
    switch (entity)
    {
        case entity_t::unknown:
            return "unknown";
        case entity_t::id:
            return "id";
        case entity_t::exception:
            return "exception";
        case entity_t::error:
            return "error";
        case entity_t::response:
            return "response";
        case entity_t::request:
            return "request";
        case entity_t::notification:
            return "notification";
        case entity_t::batch:
            return "batch";
        default:
            return "unknown";
    }
}


/////////////////////////// NullableEntity implementation /////////////////////

inline NullableEntity::NullableEntity(entity_t type) : Entity(type), isNull(false)
{
}

inline NullableEntity::NullableEntity(entity_t type, std::nullptr_t) : Entity(type), isNull(true)
{
}


/////////////////////////// Id implementation /////////////////////////////////

inline Id::Id() : Entity(entity_t::id), type_(value_t::null), int_id_(0), string_id_("")
{
}

inline Id::Id(int id) : Entity(entity_t::id), type_(value_t::integer), int_id_(id), string_id_("")
{
}

inline Id::Id(const char* id) : Entity(entity_t::id), type_(value_t::string), int_id_(0), string_id_(id)
{
}

inline Id::Id(const std::string& id) : Id(id.c_str())
{
}

inline Id::Id(const Json& json_id) : Id()
{
    Id::parse_json(json_id);
}

inline void Id::parse_json(const Json& json)
{
    if (json.is_null())
    {
        type_ = value_t::null;
    }
    else if (json.is_number_integer())
    {
        int_id_ = json.get<int>();
        type_ = value_t::integer;
    }
    else if (json.is_string())
    {
        string_id_ = json.get<std::string>();
        type_ = value_t::string;
    }
    else
        throw std::invalid_argument("id must be integer, string or null");
}

inline Json Id::to_json() const
{
    if (type_ == value_t::null)
        return nullptr;
    if (type_ == value_t::string)
        return string_id_;
    if (type_ == value_t::integer)
        return int_id_;

    return nullptr;
}


//////////////////////// Error implementation /////////////////////////////////

inline Parameter::Parameter(std::nullptr_t) : NullableEntity(entity_t::id, nullptr), type(value_t::null)
{
}

inline Parameter::Parameter(const Json& json) : NullableEntity(entity_t::id), type(value_t::null)
{
    if (json != nullptr)
        Parameter::parse_json(json);
}

inline Parameter::Parameter(const std::string& key1, const Json& value1, const std::string& key2, const Json& value2, const std::string& key3,
                            const Json& value3, const std::string& key4, const Json& value4)
    : NullableEntity(entity_t::id), type(value_t::map)
{
    param_map[key1] = value1;
    if (!key2.empty())
        add(key2, value2);
    if (!key3.empty())
        add(key3, value3);
    if (!key4.empty())
        add(key4, value4);
}

inline void Parameter::add(const std::string& key, const Json& value)
{
    param_map[key] = value;
}

inline void Parameter::parse_json(const Json& json)
{
    if (json.is_null())
    {
        param_array.clear();
        param_map.clear();
        type = value_t::null;
        isNull = true;
    }
    else if (json.is_array())
    {
        param_array = json.get<std::vector<Json>>();
        param_map.clear();
        type = value_t::array;
    }
    else
    {
        param_map = json.get<std::map<std::string, Json>>();
        param_array.clear();
        type = value_t::map;
    }
}

inline Json Parameter::to_json() const
{
    if (type == value_t::array)
        return param_array;
    if (type == value_t::map)
        return param_map;

    return nullptr;
}

inline bool Parameter::is_array() const
{
    return type == value_t::array;
}

inline bool Parameter::is_map() const
{
    return type == value_t::map;
}

inline bool Parameter::is_null() const
{
    return type == value_t::null;
}

inline bool Parameter::has(const std::string& key) const
{
    if (type != value_t::map)
        return false;
    return (param_map.find(key) != param_map.end());
}

inline Json Parameter::get(const std::string& key) const
{
    return param_map.at(key);
}

inline bool Parameter::has(size_t idx) const
{
    if (type != value_t::array)
        return false;
    return (param_array.size() > idx);
}

inline Json Parameter::get(size_t idx) const
{
    return param_array.at(idx);
}


//////////////////////// Error implementation /////////////////////////////////

inline Error::Error(const Json& json) : Error("Internal error", -32603, nullptr)
{
    if (json != nullptr)
        Error::parse_json(json);
}

inline Error::Error(std::nullptr_t) : NullableEntity(entity_t::error, nullptr), code_(0), message_(""), data_(nullptr)
{
}

inline Error::Error(const std::string& message, int code, const Json& data) : NullableEntity(entity_t::error), code_(code), message_(message), data_(data)
{
}

inline void Error::parse_json(const Json& json)
{
    try
    {
        if (json.count("code") == 0)
            throw RpcException("code is missing");
        code_ = json["code"];
        if (json.count("message") == 0)
            throw RpcException("message is missing");
        message_ = json["message"].get<std::string>();
        if (json.count("data") != 0u)
            data_ = json["data"];
        else
            data_ = nullptr;
    }
    catch (const RpcException&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw RpcException(e.what());
    }
}

inline Json Error::to_json() const
{
    Json j = {{"code", code_}, {"message", message_}};

    if (!data_.is_null())
        j["data"] = data_;
    return j;
}


////////////////////// Request implementation /////////////////////////////////

inline Request::Request(const Json& json) : Entity(entity_t::request), method_(""), id_()
{
    if (json != nullptr)
        Request::parse_json(json);
}

inline Request::Request(const Id& id, const std::string& method, const Parameter& params) : Entity(entity_t::request), method_(method), params_(params), id_(id)
{
}

inline void Request::parse_json(const Json& json)
{
    try
    {
        if (json.count("id") == 0)
            throw InvalidRequestException("id is missing");

        try
        {
            id_ = Id(json["id"]);
        }
        catch (const std::exception& e)
        {
            throw InvalidRequestException(e.what());
        }

        if (json.count("jsonrpc") == 0)
            throw InvalidRequestException("jsonrpc is missing", id_);
        std::string jsonrpc = json["jsonrpc"].get<std::string>();
        if (jsonrpc != "2.0")
            throw InvalidRequestException("invalid jsonrpc value: " + jsonrpc, id_);

        if (json.count("method") == 0)
            throw InvalidRequestException("method is missing", id_);
        if (!json["method"].is_string())
            throw InvalidRequestException("method must be a string value", id_);
        method_ = json["method"].get<std::string>();
        if (method_.empty())
            throw InvalidRequestException("method must not be empty", id_);

        if (json.count("params") != 0u)
            params_.parse_json(json["params"]);
        else
            params_ = nullptr;
    }
    catch (const RequestException&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw InternalErrorException(e.what(), id_);
    }
}

inline Json Request::to_json() const
{
    Json json = {{"jsonrpc", "2.0"}, {"method", method_}, {"id", id_.to_json()}};

    if (params_)
        json["params"] = params_.to_json();

    return json;
}


inline RpcException::RpcException(const char* text) : m_(text)
{
}

inline RpcException::RpcException(const std::string& text) : RpcException(text.c_str())
{
}

inline const char* RpcException::what() const noexcept
{
    return m_.what();
}


inline RpcEntityException::RpcEntityException(const Error& error) : RpcException(error.message()), Entity(entity_t::exception), error_(error)
{
}

inline void RpcEntityException::parse_json(const Json& /*json*/)
{
}


inline ParseErrorException::ParseErrorException(const Error& error) : RpcEntityException(error)
{
}

inline ParseErrorException::ParseErrorException(const std::string& data) : ParseErrorException(Error("Parse error", -32700, data))
{
}

inline Json ParseErrorException::to_json() const
{
    Json response = {{"jsonrpc", "2.0"}, {"error", error_.to_json()}, {"id", nullptr}};

    return response;
}


inline RequestException::RequestException(const Error& error, const Id& requestId) : RpcEntityException(error), id_(requestId)
{
}

inline Json RequestException::to_json() const
{
    Json response = {{"jsonrpc", "2.0"}, {"error", error_.to_json()}, {"id", id_.to_json()}};

    return response;
}


inline InvalidRequestException::InvalidRequestException(const Id& requestId) : RequestException(Error("Invalid request", -32600), requestId)
{
}

inline InvalidRequestException::InvalidRequestException(const Request& request) : InvalidRequestException(request.id())
{
}

inline InvalidRequestException::InvalidRequestException(const char* data, const Id& requestId)
    : RequestException(Error("Invalid request", -32600, data), requestId)
{
}

inline InvalidRequestException::InvalidRequestException(const std::string& data, const Id& requestId) : InvalidRequestException(data.c_str(), requestId)
{
}


inline MethodNotFoundException::MethodNotFoundException(const Id& requestId) : RequestException(Error("Method not found", -32601), requestId)
{
}

inline MethodNotFoundException::MethodNotFoundException(const Request& request) : MethodNotFoundException(request.id())
{
}

inline MethodNotFoundException::MethodNotFoundException(const char* data, const Id& requestId)
    : RequestException(Error("Method not found", -32601, data), requestId)
{
}

inline MethodNotFoundException::MethodNotFoundException(const std::string& data, const Id& requestId) : MethodNotFoundException(data.c_str(), requestId)
{
}


inline InvalidParamsException::InvalidParamsException(const Id& requestId) : RequestException(Error("Invalid params", -32602), requestId)
{
}

inline InvalidParamsException::InvalidParamsException(const Request& request) : InvalidParamsException(request.id())
{
}

inline InvalidParamsException::InvalidParamsException(const char* data, const Id& requestId)
    : RequestException(Error("Invalid params", -32602, data), requestId)
{
}

inline InvalidParamsException::InvalidParamsException(const std::string& data, const Id& requestId) : InvalidParamsException(data.c_str(), requestId)
{
}


inline InternalErrorException::InternalErrorException(const Id& requestId) : RequestException(Error("Internal error", -32603), requestId)
{
}

inline InternalErrorException::InternalErrorException(const Request& request) : InternalErrorException(request.id())
{
}

inline InternalErrorException::InternalErrorException(const char* data, const Id& requestId)
    : RequestException(Error("Internal error", -32603, data), requestId)
{
}

inline InternalErrorException::InternalErrorException(const std::string& data, const Id& requestId) : InternalErrorException(data.c_str(), requestId)
{
}


///////////////////// Response implementation /////////////////////////////////

inline Response::Response(const Json& json) : Entity(entity_t::response)
{
    if (json != nullptr)
        Response::parse_json(json);
}

inline Response::Response(const Id& id, const Json& result) : Entity(entity_t::response), id_(id), result_(result), error_(nullptr)
{
}

inline Response::Response(const Id& id, const Error& error) : Entity(entity_t::response), id_(id), result_(), error_(error)
{
}

inline Response::Response(const Request& request, const Json& result) : Response(request.id(), result)
{
}

inline Response::Response(const Request& request, const Error& error) : Response(request.id(), error)
{
}

inline Response::Response(const RequestException& exception) : Response(exception.id(), exception.error())
{
}

inline void Response::parse_json(const Json& json)
{
    try
    {
        error_ = nullptr;
        result_ = nullptr;
        if (json.count("jsonrpc") == 0)
            throw RpcException("jsonrpc is missing");
        std::string jsonrpc = json["jsonrpc"].get<std::string>();
        if (jsonrpc != "2.0")
            throw RpcException("invalid jsonrpc value: " + jsonrpc);
        if (json.count("id") == 0)
            throw RpcException("id is missing");
        id_ = Id(json["id"]);
        if (json.count("result") != 0u)
            result_ = json["result"];
        else if (json.count("error") != 0u)
            error_ = json["error"];
        else
            throw RpcException("response must contain result or error");
    }
    catch (const RpcException&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw RpcException(e.what());
    }
}

inline Json Response::to_json() const
{
    Json j = {{"jsonrpc", "2.0"}, {"id", id_.to_json()}};

    if (error_)
        j["error"] = error_.to_json();
    else
        j["result"] = result_;

    return j;
}


///////////////// Notification implementation /////////////////////////////////

inline Notification::Notification(const Json& json) : Entity(entity_t::notification)
{
    if (json != nullptr)
        Notification::parse_json(json);
}

inline Notification::Notification(const char* method, const Parameter& params) : Entity(entity_t::notification), method_(method), params_(params)
{
}

inline Notification::Notification(const std::string& method, const Parameter& params) : Notification(method.c_str(), params)
{
}

inline void Notification::parse_json(const Json& json)
{
    try
    {
        if (json.count("jsonrpc") == 0)
            throw RpcException("jsonrpc is missing");
        std::string jsonrpc = json["jsonrpc"].get<std::string>();
        if (jsonrpc != "2.0")
            throw RpcException("invalid jsonrpc value: " + jsonrpc);

        if (json.count("method") == 0)
            throw RpcException("method is missing");
        if (!json["method"].is_string())
            throw RpcException("method must be a string value");
        method_ = json["method"].get<std::string>();
        if (method_.empty())
            throw RpcException("method must not be empty");

        if (json.count("params") != 0u)
            params_.parse_json(json["params"]);
        else
            params_ = nullptr;
    }
    catch (const RpcException&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw RpcException(e.what());
    }
}

inline Json Notification::to_json() const
{
    Json json = {{"jsonrpc", "2.0"}, {"method", method_}};

    if (params_)
        json["params"] = params_.to_json();

    return json;
}


//////////////////////// Batch implementation /////////////////////////////////

inline Batch::Batch(const Json& json) : Entity(entity_t::batch)
{
    if (json != nullptr)
        Batch::parse_json(json);
}

inline void Batch::parse_json(const Json& json)
{
    //	cout << "Batch::parse: " << json.dump() << "\n";
    entities.clear();
    for (const auto& it : json)
    {
        //		cout << "x: " << it->dump() << "\n";
        entity_ptr entity(nullptr);
        try
        {
            entity = Parser::do_parse_json(it);
            if (!entity)
                entity = std::make_shared<Error>("Invalid Request", -32600);
        }
        catch (const RequestException& e)
        {
            entity = std::make_shared<RequestException>(e);
        }
        catch (const std::exception& e)
        {
            entity = std::make_shared<Error>(e.what(), -32600);
        }
        entities.push_back(entity);
    }
    if (entities.empty())
        throw InvalidRequestException();
}

inline Json Batch::to_json() const
{
    Json result;
    for (const auto& j : entities)
        result.push_back(j->to_json());
    return result;
}


//////////////////////// Parser implementation ////////////////////////////////

inline void Parser::register_notification_callback(const std::string& notification, notification_callback callback)
{
    if (callback)
        notification_callbacks_[notification] = callback;
}

inline void Parser::register_request_callback(const std::string& request, request_callback callback)
{
    if (callback)
        request_callbacks_[request] = callback;
}

inline entity_ptr Parser::parse(const std::string& json_str)
{
    // std::cout << "parse: " << json_str << "\n";
    entity_ptr entity = do_parse(json_str);
    if (entity && entity->is_notification())
    {
        notification_ptr notification = std::dynamic_pointer_cast<jsonrpcpp::Notification>(entity);
        if (notification_callbacks_.find(notification->method()) != notification_callbacks_.end())
        {
            notification_callback callback = notification_callbacks_[notification->method()];
            if (callback)
                callback(notification->params());
        }
    }
    else if (entity && entity->is_request())
    {
        request_ptr request = std::dynamic_pointer_cast<jsonrpcpp::Request>(entity);
        if (request_callbacks_.find(request->method()) != request_callbacks_.end())
        {
            request_callback callback = request_callbacks_[request->method()];
            if (callback)
            {
                jsonrpcpp::response_ptr response = callback(request->id(), request->params());
                if (response)
                    return response;
            }
        }
    }
    return entity;
}

inline entity_ptr Parser::parse_json(const Json& json)
{
    return do_parse_json(json);
}

inline entity_ptr Parser::do_parse(const std::string& json_str)
{
    try
    {
        return do_parse_json(Json::parse(json_str));
    }
    catch (const RpcException&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw ParseErrorException(e.what());
    }

    return nullptr;
}

inline entity_ptr Parser::do_parse_json(const Json& json)
{
    try
    {
        if (is_request(json))
            return std::make_shared<Request>(json);
        if (is_notification(json))
            return std::make_shared<Notification>(json);
        if (is_response(json))
            return std::make_shared<Response>(json);
        if (is_batch(json))
            return std::make_shared<Batch>(json);
    }
    catch (const RpcException&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        throw RpcException(e.what());
    }

    return nullptr;
}

inline bool Parser::is_request(const std::string& json_str)
{
    try
    {
        return is_request(Json::parse(json_str));
    }
    catch (const std::exception&)
    {
        return false;
    }
}

inline bool Parser::is_request(const Json& json)
{
    return ((json.count("method") != 0u) && (json.count("id") != 0u));
}

inline bool Parser::is_notification(const std::string& json_str)
{
    try
    {
        return is_notification(Json::parse(json_str));
    }
    catch (const std::exception&)
    {
        return false;
    }
}

inline bool Parser::is_notification(const Json& json)
{
    return ((json.count("method") != 0u) && (json.count("id") == 0));
}

inline bool Parser::is_response(const std::string& json_str)
{
    try
    {
        return is_response(Json::parse(json_str));
    }
    catch (const std::exception&)
    {
        return false;
    }
}

inline bool Parser::is_response(const Json& json)
{
    return (((json.count("result") != 0u) || (json.count("error") != 0u)) && (json.count("id") != 0u));
}

inline bool Parser::is_batch(const std::string& json_str)
{
    try
    {
        return is_batch(Json::parse(json_str));
    }
    catch (const std::exception&)
    {
        return false;
    }
}

inline bool Parser::is_batch(const Json& json)
{
    return (json.is_array());
}

} // namespace jsonrpcpp

#endif
