/***
       __  ____   __   __ _  ____  ____   ___  _    _   
     _(  )/ ___) /  \ (  ( \(  _ \(  _ \ / __)( )  ( )  
    / \) \\___ \(  O )/    / )   / ) __/( (__(_ _)(_ _) 
    \____/(____/ \__/ \_)__)(__\_)(__)   \___)(_)  (_)  
    version 1.1.0
    https://github.com/badaix/jsonrpcpp

    This file is part of jsonrpc++
    Copyright (C) 2017  Johannes Pohl
    
    This software may be modified and distributed under the terms
    of the MIT license.  See the LICENSE file for details.
***/

/// http://patorjk.com/software/taag/#p=display&f=Graceful&t=JSONRPC%2B%2B

#ifndef JSON_RPC_H
#define JSON_RPC_H

#include <string>
#include <cstring>
#include <vector>
#include <exception>
#include "common/json.hpp"


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


typedef std::shared_ptr<Entity> entity_ptr;
typedef std::shared_ptr<Request> request_ptr;
typedef std::shared_ptr<Notification> notification_ptr;
typedef std::shared_ptr<Parameter> parameter_ptr;
typedef std::shared_ptr<Response> response_ptr;
typedef std::shared_ptr<Error> error_ptr;
typedef std::shared_ptr<Batch> batch_ptr;



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
	virtual ~Entity();
	
	bool is_exception();
	bool is_id();
	bool is_error();
	bool is_response();
	bool is_request();
	bool is_notification();
	bool is_batch();

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
	virtual ~NullableEntity();
	virtual explicit operator bool() const
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

	virtual Json to_json() const;
	virtual void parse_json(const Json& json);

	friend std::ostream& operator<< (std::ostream &out, const Id &id)
	{
		out << id.to_json();
		return out;
	}

	value_t type;
	int int_id;
	std::string string_id;
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
	Parameter(const std::string& key1, const Json& value1, 
	          const std::string& key2 = "", const Json& value2 = nullptr, 
	          const std::string& key3 = "", const Json& value3 = nullptr, 
	          const std::string& key4 = "", const Json& value4 = nullptr);

	virtual Json to_json() const;
	virtual void parse_json(const Json& json);

	bool is_array() const;
	bool is_map() const;
	bool is_null() const;

	Json get(const std::string& key) const;
	Json get(size_t idx) const;
	bool has(const std::string& key) const;
	bool has(size_t idx) const;

	template<typename T>
	T get(const std::string& key) const
	{
		return get(key).get<T>();
	}

	template<typename T>
	T get(size_t idx) const
	{
		return get(idx).get<T>();
	}

	template<typename T>
	T get(const std::string& key, const T& default_value) const
	{
		if (!has(key))
			return default_value;
		else
			return get<T>(key);
	}

	template<typename T>
	T get(size_t idx, const T& default_value) const
	{
		if (!has(idx))
			return default_value;
		else
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

	virtual Json to_json() const;
	virtual void parse_json(const Json& json);

	int code;
	std::string message;
	Json data;
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

	virtual Json to_json() const;
	virtual void parse_json(const Json& json);

	std::string method;
	Parameter params;
	Id id;
};





class RpcException : public std::exception
{
  char* text_;
public:
	RpcException(const char* text);
	RpcException(const std::string& text);
	RpcException(const RpcException& e);

	virtual ~RpcException() throw();
	virtual const char* what() const noexcept;
};





class ParseErrorException : public RpcException, public Entity
{
public:
	Error error;

	ParseErrorException(const Error& error);
	ParseErrorException(const ParseErrorException& e);
	ParseErrorException(const std::string& data);
	virtual Json to_json() const;

protected:
	virtual void parse_json(const Json& json);
};



//	-32600	Invalid Request	The JSON sent is not a valid Request object.
//	-32601	Method not found	The method does not exist / is not available.
//	-32602	Invalid params	Invalid method parameter(s).
//	-32603	Internal error	Internal JSON-RPC error.

class RequestException : public RpcException, public Entity
{
public:
	Error error;
	Id id;

	RequestException(const Error& error, const Id& requestId = Id());
	RequestException(const RequestException& e);
	virtual Json to_json() const;

protected:
	virtual void parse_json(const Json& json);
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
	Id id;
	Json result;
	Error error;

	Response(const Json& json = nullptr);
	Response(const Id& id, const Json& result);
	Response(const Id& id, const Error& error);
	Response(const Request& request, const Json& result);
	Response(const Request& request, const Error& error);
	Response(const RequestException& exception);

	virtual Json to_json() const;
	virtual void parse_json(const Json& json);
};





class Notification : public Entity
{
public:
	std::string method;
	Parameter params;
	Notification(const Json& json = nullptr);
	Notification(const char* method, const Parameter& params = nullptr);
	Notification(const std::string& method, const Parameter& params);

	virtual Json to_json() const;
	virtual void parse_json(const Json& json);
};




typedef std::function<void(const Parameter& params)> notification_callback;
typedef std::function<jsonrpcpp::response_ptr(const Id& id, const Parameter& params)> request_callback;


class Parser
{
public:
	Parser();
	virtual ~Parser();
	
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

	virtual Json to_json() const;
	virtual void parse_json(const Json& json);

	template<typename T>
	void add(const T& entity)
	{
		entities.push_back(std::make_shared<T>(entity));
	}

	void add_ptr(const entity_ptr& entity)
	{
		entities.push_back(entity);
	}
};



} //namespace jsonrpc



#endif
