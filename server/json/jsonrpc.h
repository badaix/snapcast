/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#ifndef JSON_RPC_H
#define JSON_RPC_H

#include <string>
#include <vector>
#include "externals/json.hpp"
#include "common/snapException.h"


using Json = nlohmann::json;

namespace jsonrpc
{

class Entity
{
public:
	Entity()
	{
	}

	virtual ~Entity()
	{
	}

	virtual Json to_json() const = 0;
};



struct Id
{
	enum class value_t : uint8_t
	{
		null = 0,
		string,
		integer
	};

	Id() : type(value_t::null), int_id(0), string_id("")
	{
	}

	Id(int id) : type(value_t::integer), int_id(id), string_id("")
	{
	}

	Id(const std::string& id) : type(value_t::string), int_id(0), string_id(id)
	{
	}

	Id(const Json& json_id) : type(value_t::null)
	{
		if (json_id.is_null())
		{
			type = value_t::null;
		}
		else if (json_id.is_number_integer())
		{
			int_id = json_id.get<int>();
			type = value_t::integer;
		}
		else if (json_id.is_string())
		{
			string_id = json_id.get<std::string>();
			type = value_t::string;
		}
		else
			throw std::invalid_argument("id must be integer, string or null");
	}

	Json to_json() const
	{
		if (type == value_t::string)
			return string_id;
		else if (type == value_t::integer)
			return int_id;
		else
			return nullptr;
	}

	friend std::ostream& operator<< (std::ostream &out, const Id &id)
	{
		out << id.to_json();
		return out;
	}

	value_t type;
	int int_id;
	std::string string_id;
};



class Error : public Entity
{
public:
	Error(int code, const std::string& message, const Json& data = nullptr) : Entity(), code(code), message(message), data(data)
	{
	}

	Error(const std::string& message, const Json& data = nullptr) : Error(-32603, message, data)
	{
	}

	virtual Json to_json() const
	{
		Json j = {
				{"code", code},
				{"message", message},
			};

		if (!data.is_null())
			j["data"] = data;
		return j;
	}

	int code;
	std::string message;
	Json data;
};


class RequestException : public SnapException
{
  Error error_;
  Id id_;
public:
	RequestException(const char* text, int errorCode = 0, const Id& requestId = Id()) : SnapException(text), error_(text, errorCode), id_(requestId)
	{
	}

	RequestException(const std::string& text, int errorCode = 0, const Id& requestId = Id()) : SnapException(text), error_(text, errorCode), id_(requestId)
	{
	}

	RequestException(const RequestException& e) :  SnapException(e.what()), error_(error()), id_(e.id_)
	{
	}

	virtual Error error() const noexcept
	{
		return error_;
	}

	Json getResponse() const noexcept
	{
		Json response = {
			{"jsonrpc", "2.0"},
			{"error", error_.to_json()},
			{"id", id_.to_json()}
		};

		return response;
	}
};


//	-32600	Invalid Request	The JSON sent is not a valid Request object.
//	-32601	Method not found	The method does not exist / is not available.
//	-32602	Invalid params	Invalid method parameter(s).
//	-32603	Internal error	Internal JSON-RPC error.

class InvalidRequestException : public RequestException
{
public:
	InvalidRequestException(const Id& requestId = Id()) : RequestException("invalid request", -32600, requestId)
	{
	}

	InvalidRequestException(const std::string& message, const Id& requestId = Id()) : RequestException(message, -32600, requestId)
	{
	}
};



class MethodNotFoundException : public RequestException
{
public:
	MethodNotFoundException(const Id& requestId = Id()) : RequestException("method not found", -32601, requestId)
	{
	}

	MethodNotFoundException(const std::string& message, const Id& requestId = Id()) : RequestException(message, -32601, requestId)
	{
	}
};



class InvalidParamsException : public RequestException
{
public:
	InvalidParamsException(const Id& requestId = Id()) : RequestException("invalid params", -32602, requestId)
	{
	}

	InvalidParamsException(const std::string& message, const Id& requestId = Id()) : RequestException(message, -32602, requestId)
	{
	}
};


class InternalErrorException : public RequestException
{
public:
	InternalErrorException(const Id& requestId = Id()) : RequestException("internal error", -32603, requestId)
	{
	}

	InternalErrorException(const std::string& message, const Id& requestId = Id()) : RequestException(message, -32603, requestId)
	{
	}
};




/*
class Response
{
public:
	Response();

	void parse(const std::string& json);
	std::string result;
	Error error;
	Id id;


protected:
	Json json_;

};
*/

/// JSON-RPC 2.0 request
/**
 * Simple jsonrpc 2.0 parser with getters
 * Currently no named parameters are supported, but only array parameters
 */
class Request
{
public:
	Request();

	void parse(const std::string& json);
	std::string method;
	std::map<std::string, Json> params;
	Id id;

	Json getResponse(const Json& result);
	Json getError(int code, const std::string& message);

	Json getParam(const std::string& key);
	bool hasParam(const std::string& key);

	template<typename T>
	T getParam(const std::string& key, const T& lowerRange, const T& upperRange)
	{
		T value = getParam(key).get<T>();
		if (value < lowerRange)
			throw InvalidParamsException(key + " out of range", id);
		else if (value > upperRange)
			throw InvalidParamsException(key + " out of range", id);

		return value;
	}


protected:
	Json json_;

};



class Notification
{
public:
	static Json getJson(const std::string& method, const Json& data);

};


class Batch
{

};


}





#endif
