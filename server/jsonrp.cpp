/***
    This file is part of jsonrpc++
    Copyright (C) 2017 Johannes Pohl
    
    This software may be modified and distributed under the terms
    of the MIT license.  See the LICENSE file for details.
***/

#include "jsonrp.hpp"


using namespace std;

namespace jsonrpcpp
{


/////////////////////////// Entity implementation /////////////////////////////

Entity::Entity(entity_t type) : entity(type)
{
}


Entity::~Entity()
{
}


bool Entity::is_exception()
{
	return (entity == entity_t::exception);
}


bool Entity::is_id()
{
	return (entity == entity_t::id);
}


bool Entity::is_error()
{
	return (entity == entity_t::error);
}


bool Entity::is_response()
{
	return (entity == entity_t::response);
}


bool Entity::is_request()
{
	return (entity == entity_t::request);
}


bool Entity::is_notification()
{
	return (entity == entity_t::notification);
}


bool Entity::is_batch()
{
	return (entity == entity_t::batch);
}


void Entity::parse(const char* json_str)
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
	catch (const RpcException& e)
	{
		throw;
	}
	catch (const exception& e)
	{
		throw ParseErrorException(e.what());
	}
}


void Entity::parse(const std::string& json_str)
{
	parse(json_str.c_str());
}


std::string Entity::type_str() const
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

NullableEntity::NullableEntity(entity_t type) : Entity(type), isNull(false)
{
}


NullableEntity::NullableEntity(entity_t type, std::nullptr_t) : Entity(type), isNull(true)
{
}


NullableEntity::~NullableEntity()
{
};





/////////////////////////// Id implementation /////////////////////////////////

Id::Id() : Entity(entity_t::id), type(value_t::null), int_id(0), string_id("")
{
}


Id::Id(int id) : Entity(entity_t::id), type(value_t::integer), int_id(id), string_id("")
{
}


Id::Id(const char* id) : Entity(entity_t::id), type(value_t::string), int_id(0), string_id(id)
{
}


Id::Id(const std::string& id) : Id(id.c_str())
{
}


Id::Id(const Json& json_id) : Entity(entity_t::id), type(value_t::null)
{
	parse_json(json_id);
}


void Id::parse_json(const Json& json)
{
	if (json.is_null())
	{
		type = value_t::null;
	}
	else if (json.is_number_integer())
	{
		int_id = json.get<int>();
		type = value_t::integer;
	}
	else if (json.is_string())
	{
		string_id = json.get<std::string>();
		type = value_t::string;
	}
	else
		throw std::invalid_argument("id must be integer, string or null");
}


Json Id::to_json() const
{
	if (type == value_t::null)
		return nullptr;
	else if (type == value_t::string)
		return string_id;
	else if (type == value_t::integer)
		return int_id;

	return nullptr;
}





//////////////////////// Error implementation /////////////////////////////////

Parameter::Parameter(std::nullptr_t) : NullableEntity(entity_t::id, nullptr), type(value_t::null)
{
}


Parameter::Parameter(const Json& json) : NullableEntity(entity_t::id), type(value_t::null)
{
	if (json != nullptr)
		parse_json(json);
}


Parameter::Parameter(const std::string& key1, const Json& value1, 
                     const std::string& key2, const Json& value2, 
                     const std::string& key3, const Json& value3, 
                     const std::string& key4, const Json& value4) : NullableEntity(entity_t::id), type(value_t::map)
{
	param_map[key1] = value1;
	if (!key2.empty())
		param_map[key2] = value2;
	if (!key3.empty())
		param_map[key3] = value3;
	if (!key4.empty())
		param_map[key4] = value4;
}


void Parameter::parse_json(const Json& json)
{
	if (json.is_array())
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


Json Parameter::to_json() const
{
	if (type == value_t::array)
		return param_array;
	else if (type == value_t::map)
		return param_map;
	else
		return nullptr;
}


bool Parameter::is_array() const
{
	return type == value_t::array;
}


bool Parameter::is_map() const
{
	return type == value_t::map;
}


bool Parameter::is_null() const
{
	return isNull;
}


bool Parameter::has(const std::string& key) const
{
	if (type != value_t::map)
		return false;
	return (param_map.find(key) != param_map.end());
}


Json Parameter::get(const std::string& key) const
{
	return param_map.at(key);
}


bool Parameter::has(size_t idx) const
{
	if (type != value_t::array)
		return false;
	return (param_array.size() > idx);
}


Json Parameter::get(size_t idx) const
{
	return param_array.at(idx);
}





//////////////////////// Error implementation /////////////////////////////////

Error::Error(const Json& json) : Error("Internal error", -32603, nullptr)
{
	if (json != nullptr)
		parse_json(json);
}


Error::Error(std::nullptr_t) : NullableEntity(entity_t::error, nullptr), code(0), message(""), data(nullptr)
{
}


Error::Error(const std::string& message, int code, const Json& data) : NullableEntity(entity_t::error), code(code), message(message), data(data)
{
}


void Error::parse_json(const Json& json)
{
	try
	{
		if (json.count("code") == 0)
			throw RpcException("code is missing");
		code = json["code"];
		if (json.count("message") == 0)
			throw RpcException("message is missing");
		message = json["message"];
		if (json.count("data"))
			data = json["data"];
		else
			data = nullptr;
	}
	catch (const RpcException& e)
	{
		throw;
	}
	catch (const exception& e)
	{
		throw RpcException(e.what());
	}
}


Json Error::to_json() const
{
	Json j = {
			{"code", code},
			{"message", message},
		};

	if (!data.is_null())
		j["data"] = data;
	return j;
}





////////////////////// Request implementation /////////////////////////////////

Request::Request(const Json& json) : Entity(entity_t::request), method(""), id()
{
	if (json != nullptr)
		parse_json(json);
}


Request::Request(const Id& id, const std::string& method, const Parameter& params) : Entity(entity_t::request), method(method), params(params), id(id)
{
}


void Request::parse_json(const Json& json)
{
	try
	{
		if (json.count("id") == 0)
			throw InvalidRequestException("id is missing");

		try
		{
			id = Id(json["id"]);
		}
		catch(const std::exception& e)
		{
			throw InvalidRequestException(e.what());
		}

		if (json.count("jsonrpc") == 0)
			throw InvalidRequestException("jsonrpc is missing", id);
		string jsonrpc = json["jsonrpc"].get<string>();
		if (jsonrpc != "2.0")
			throw InvalidRequestException("invalid jsonrpc value: " + jsonrpc, id);

		if (json.count("method") == 0)
			throw InvalidRequestException("method is missing", id);
		if (!json["method"].is_string())
			throw InvalidRequestException("method must be a string value", id);
		method = json["method"];
		if (method.empty())
			throw InvalidRequestException("method must not be empty", id);

		if (json.count("params"))
			params.parse_json(json["params"]);
		else
			params = nullptr;
	}
	catch (const RequestException& e)
	{
		throw;
	}
	catch (const exception& e)
	{
		throw InternalErrorException(e.what(), id);
	}
}


Json Request::to_json() const
{
	Json json = {
		{"jsonrpc", "2.0"},
		{"method", method},
		{"id", id.to_json()}
	};

	if (params)
		json["params"] = params.to_json();
	
	return json;
}



RpcException::RpcException(const char* text)
{
	text_ = new char[std::strlen(text) + 1];
	std::strcpy(text_, text);
}

RpcException::RpcException(const std::string& text) : RpcException(text.c_str())
{
}

RpcException::RpcException(const RpcException& e) : RpcException(e.what())
{
}

RpcException::~RpcException() throw()
{
	delete[] text_;
}

const char* RpcException::what() const noexcept
{
	return text_;
}




ParseErrorException::ParseErrorException(const Error& error) : RpcException(error.message), Entity(entity_t::exception), error(error)
{
}

ParseErrorException::ParseErrorException(const ParseErrorException& e) : RpcException(e.what()), Entity(entity_t::exception), error(e.error)
{
}

ParseErrorException::ParseErrorException(const std::string& data) : ParseErrorException(Error("Parse error", -32700, data))
{
}

Json ParseErrorException::to_json() const
{
	Json response = {
		{"jsonrpc", "2.0"},
		{"error", error.to_json()},
		{"id", nullptr}
	};

	return response;
}

void ParseErrorException::parse_json(const Json& json)
{
}




RequestException::RequestException(const Error& error, const Id& requestId) : RpcException(error.message), Entity(entity_t::exception), error(error), id(requestId)
{
}

RequestException::RequestException(const RequestException& e) :  RpcException(e.what()), Entity(entity_t::exception), error(e.error), id(e.id)
{
}

Json RequestException::to_json() const
{
	Json response = {
		{"jsonrpc", "2.0"},
		{"error", error.to_json()},
		{"id", id.to_json()}
	};

	return response;
}

void RequestException::parse_json(const Json& json)
{
}





InvalidRequestException::InvalidRequestException(const Id& requestId) : RequestException(Error("Invalid request", -32600), requestId)
{
}

InvalidRequestException::InvalidRequestException(const Request& request) : InvalidRequestException(request.id)
{
}

InvalidRequestException::InvalidRequestException(const char* data, const Id& requestId) : RequestException(Error("Invalid request", -32600, data), requestId)
{
}

InvalidRequestException::InvalidRequestException(const std::string& data, const Id& requestId) : InvalidRequestException(data.c_str(), requestId)
{
}



MethodNotFoundException::MethodNotFoundException(const Id& requestId) : RequestException(Error("Method not found", -32601), requestId)
{
}

MethodNotFoundException::MethodNotFoundException(const Request& request) : MethodNotFoundException(request.id)
{
}

MethodNotFoundException::MethodNotFoundException(const char* data, const Id& requestId) : RequestException(Error("Method not found", -32601, data), requestId)
{
}

MethodNotFoundException::MethodNotFoundException(const std::string& data, const Id& requestId) : MethodNotFoundException(data.c_str(), requestId)
{
}



InvalidParamsException::InvalidParamsException(const Id& requestId) : RequestException(Error("Invalid params", -32602), requestId)
{
}

InvalidParamsException::InvalidParamsException(const Request& request) : InvalidParamsException(request.id)
{
}

InvalidParamsException::InvalidParamsException(const char* data, const Id& requestId) : RequestException(Error("Invalid params", -32602, data), requestId)
{
}

InvalidParamsException::InvalidParamsException(const std::string& data, const Id& requestId) : InvalidParamsException(data.c_str(), requestId)
{
}



InternalErrorException::InternalErrorException(const Id& requestId) : RequestException(Error("Internal error", -32603), requestId)
{
}

InternalErrorException::InternalErrorException(const Request& request) : InternalErrorException(request.id)
{
}

InternalErrorException::InternalErrorException(const char* data, const Id& requestId) : RequestException(Error("Internal error", -32603, data), requestId)
{
}

InternalErrorException::InternalErrorException(const std::string& data, const Id& requestId) : InternalErrorException(data.c_str(), requestId)
{
}



///////////////////// Response implementation /////////////////////////////////

Response::Response(const Json& json) : Entity(entity_t::response)
{
	if (json != nullptr)
		parse_json(json);
}


Response::Response(const Id& id, const Json& result) : Entity(entity_t::response), id(id), result(result), error(nullptr)
{
}


Response::Response(const Id& id, const Error& error) : Entity(entity_t::response), id(id), result(), error(error)
{
}


Response::Response(const Request& request, const Json& result) : Response(request.id, result)
{
}


Response::Response(const Request& request, const Error& error) : Response(request.id, error)
{
}


Response::Response(const RequestException& exception) : Response(exception.id, exception.error)
{
}


void Response::parse_json(const Json& json)
{
	try
	{
		error = nullptr;
		result = nullptr;
		if (json.count("jsonrpc") == 0)
			throw RpcException("jsonrpc is missing");
		string jsonrpc = json["jsonrpc"].get<string>();
		if (jsonrpc != "2.0")
			throw RpcException("invalid jsonrpc value: " + jsonrpc);
		if (json.count("id") == 0)
			throw RpcException("id is missing");
		id = Id(json["id"]);
		if (json.count("result"))
			result = json["result"];
		else if (json.count("error"))
			error.parse_json(json["error"]);
		else
			throw RpcException("response must contain result or error");			
	}
	catch (const RpcException& e)
	{
		throw;
	}
	catch (const exception& e)
	{
		throw RpcException(e.what());
	}
}


Json Response::to_json() const
{
	Json j = {
		{"jsonrpc", "2.0"},
		{"id", id.to_json()},
	};

	if (error)
		j["error"] = error.to_json();
	else
		j["result"] = result;
	
	return j;
}





///////////////// Notification implementation /////////////////////////////////

Notification::Notification(const Json& json) : Entity(entity_t::notification)
{
	if (json != nullptr)
		parse_json(json);
}


Notification::Notification(const char* method, const Parameter& params) : Entity(entity_t::notification), method(method), params(params)
{
}


Notification::Notification(const std::string& method, const Parameter& params) : Notification(method.c_str(), params)
{
}


void Notification::parse_json(const Json& json)
{
	try
	{
		if (json.count("jsonrpc") == 0)
			throw RpcException("jsonrpc is missing");
		string jsonrpc = json["jsonrpc"].get<string>();
		if (jsonrpc != "2.0")
			throw RpcException("invalid jsonrpc value: " + jsonrpc);

		if (json.count("method") == 0)
			throw RpcException("method is missing");
		if (!json["method"].is_string())
			throw RpcException("method must be a string value");
		method = json["method"];
		if (method.empty())
			throw RpcException("method must not be empty");

		if (json.count("params"))
			params.parse_json(json["params"]);
		else
			params = nullptr;
	}
	catch (const RpcException& e)
	{
		throw;
	}
	catch (const exception& e)
	{
		throw RpcException(e.what());
	}
}


Json Notification::to_json() const
{
	Json json = {
		{"jsonrpc", "2.0"},
		{"method", method},
	};

	if (params)
		json["params"] = params.to_json();

	return json;
}





//////////////////////// Batch implementation /////////////////////////////////

Batch::Batch(const Json& json) : Entity(entity_t::batch)
{
	if (json != nullptr)
		parse_json(json);
}


void Batch::parse_json(const Json& json)
{
//	cout << "Batch::parse: " << json.dump() << "\n";
	entities.clear();
	for (auto it = json.begin(); it != json.end(); ++it) 
	{
//		cout << "x: " << it->dump() << "\n";
		entity_ptr entity(nullptr);
		try
		{
			entity = Parser::do_parse_json(*it);
			if (!entity)
				entity = make_shared<Error>("Invalid Request", -32600);
		}
		catch(const RequestException& e)
		{
			entity = make_shared<RequestException>(e);
		}
		catch(const std::exception& e)
		{
			entity = make_shared<Error>(e.what(), -32600);
		}
		entities.push_back(entity);
	}
	if (entities.empty())
		throw InvalidRequestException();
}


Json Batch::to_json() const
{
	Json result;
	for (const auto& j: entities)
		result.push_back(j->to_json());
	return result;
}


/*void Batch::add(const entity_ptr entity)
{
	entities.push_back(entity);
}
*/




//////////////////////// Parser implementation ////////////////////////////////

Parser::Parser()
{

}


Parser::~Parser()
{

}


void Parser::register_notification_callback(const std::string& notification, notification_callback callback)
{
	if (callback)
		notification_callbacks_[notification] = callback;
}


void Parser::register_request_callback(const std::string& request, request_callback callback)
{
	if (callback)
		request_callbacks_[request] = callback;
}


entity_ptr Parser::parse(const std::string& json_str)
{
	//std::cout << "parse: " << json_str << "\n";
	entity_ptr entity = do_parse(json_str);
	if (entity && entity->is_notification())
	{
		notification_ptr notification = dynamic_pointer_cast<jsonrpcpp::Notification>(entity);
		if (notification_callbacks_.find(notification->method) != notification_callbacks_.end())
		{
			notification_callback callback = notification_callbacks_[notification->method];
			if (callback)
				callback(notification->params);
		}
	}
	else if (entity && entity->is_request())
	{
		request_ptr request = dynamic_pointer_cast<jsonrpcpp::Request>(entity);
		if (request_callbacks_.find(request->method) != request_callbacks_.end())
		{
			request_callback callback = request_callbacks_[request->method];
			if (callback)
			{
				jsonrpcpp::response_ptr response = callback(request->id, request->params);
				if (response)
					return response;
			}
		}
	}
	return entity;
}


entity_ptr Parser::parse_json(const Json& json)
{
	return do_parse_json(json);
}


entity_ptr Parser::do_parse(const std::string& json_str)
{
	try
	{
		return do_parse_json(Json::parse(json_str));
	}
	catch (const RpcException& e)
	{
		throw;
	}
	catch (const exception& e)
	{
		throw ParseErrorException(e.what());
	}

	return nullptr;
}


entity_ptr Parser::do_parse_json(const Json& json)
{
	try
	{
		if (is_request(json))
			return make_shared<Request>(json);
		else if (is_notification(json))
			return make_shared<Notification>(json);
		else if (is_response(json))
			return make_shared<Response>(json);
		else if (is_batch(json))
			return make_shared<Batch>(json);
	}
	catch (const RpcException& e)
	{
		throw;
	}
	catch (const exception& e)
	{
		throw RpcException(e.what());
	}

	return nullptr;
}


bool Parser::is_request(const std::string& json_str)
{
	try
	{
		return is_request(Json::parse(json_str));
	}
	catch (const exception& e)
	{
		return false;
	}
}


bool Parser::is_request(const Json& json)
{
	return (json.count("method") && json.count("id"));
}


bool Parser::is_notification(const std::string& json_str)
{
	try
	{
		return is_notification(Json::parse(json_str));
	}
	catch (const exception& e)
	{
		return false;
	}
}


bool Parser:: is_notification(const Json& json)
{
	return (json.count("method") && (json.count("id") == 0));
}


bool Parser::is_response(const std::string& json_str)
{
	try
	{
		return is_response(Json::parse(json_str));
	}
	catch (const exception& e)
	{
		return false;
	}
}


bool Parser::is_response(const Json& json)
{
	return (json.count("result") && json.count("id"));
}


bool Parser::is_batch(const std::string& json_str)
{
	try
	{
		return is_batch(Json::parse(json_str));
	}
	catch (const exception& e)
	{
		return false;
	}
}


bool Parser::is_batch(const Json& json)
{
	return (json.is_array());
}



}


