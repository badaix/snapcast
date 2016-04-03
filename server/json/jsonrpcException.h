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

#ifndef JSON_RPC_EXCEPTION_H
#define JSON_RPC_EXCEPTION_H

#include <string>
#include "externals/json.hpp"
#include "common/snapException.h"


using Json = nlohmann::json;



class JsonRequestException : public SnapException
{
  int errorCode_, id_;
public:
	JsonRequestException(const char* text, int errorCode = 0, int requestId = -1) : SnapException(text), errorCode_(errorCode), id_(requestId)
	{
	}

	JsonRequestException(const std::string& text, int errorCode = 0, int requestId = -1) : SnapException(text), errorCode_(errorCode), id_(requestId)
	{
	}

//	JsonRequestException(const JsonRequest& request, const std::string& text, int errorCode = 0) : SnapException(text), errorCode_(errorCode), id_(request.id)
//	{
//	}

	JsonRequestException(const JsonRequestException& e) :  SnapException(e.what()), errorCode_(e.errorCode()), id_(e.id_)
	{
	}

	virtual int errorCode() const noexcept
	{
		return errorCode_;
	}

	Json getResponse() const noexcept
	{
		int errorCode = errorCode_;
		if (errorCode == 0)
			errorCode = -32603;

		Json response = {
			{"jsonrpc", "2.0"},
			{"error", {
				{"code", errorCode},
				{"message", what()}
			}},
		};
		if (id_ == -1)
			response["id"] = nullptr;
		else
			response["id"] = id_;

		return response;
	}
};


//	-32600	Invalid Request	The JSON sent is not a valid Request object.
//	-32601	Method not found	The method does not exist / is not available.
//	-32602	Invalid params	Invalid method parameter(s).
//	-32603	Internal error	Internal JSON-RPC error.

class JsonInvalidRequestException : public JsonRequestException
{
public:
	JsonInvalidRequestException(int requestId = -1) : JsonRequestException("invalid request", -32600, requestId)
	{
	}

	JsonInvalidRequestException(const std::string& message, int requestId = -1) : JsonRequestException(message, -32600, requestId)
	{
	}
};



class JsonMethodNotFoundException : public JsonRequestException
{
public:
	JsonMethodNotFoundException(int requestId = -1) : JsonRequestException("method not found", -32601, requestId)
	{
	}

	JsonMethodNotFoundException(const std::string& message, int requestId = -1) : JsonRequestException(message, -32601, requestId)
	{
	}
};



class JsonInvalidParamsException : public JsonRequestException
{
public:
	JsonInvalidParamsException(int requestId = -1) : JsonRequestException("invalid params", -32602, requestId)
	{
	}

	JsonInvalidParamsException(const std::string& message, int requestId = -1) : JsonRequestException(message, -32602, requestId)
	{
	}
};


class JsonInternalErrorException : public JsonRequestException
{
public:
	JsonInternalErrorException(int requestId = -1) : JsonRequestException("internal error", -32603, requestId)
	{
	}

	JsonInternalErrorException(const std::string& message, int requestId = -1) : JsonRequestException(message, -32603, requestId)
	{
	}
};


#endif

