/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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
#include <boost/lexical_cast.hpp>
#include "json.hpp"
#include "common/snapException.h"


using Json = nlohmann::json;


class JsonInvalidParamsException;


/// JSON-RPC 2.0 request
/**
 * Simple jsonrpc 2.0 parser with getters
 * Currently no named parameters are supported, but only array parameters
 */
class JsonRequest
{
public:
	JsonRequest();

	void parse(const std::string& json);
	int id;
	std::string method;
	std::vector<std::string> params;

	Json getResponse(const Json& result);
	Json getError(int code, const std::string& message);

	template<typename T>
	T getParam(size_t idx)
	{
		if (idx >= params.size())
			throw JsonInvalidParamsException(*this);
		try
		{
			return boost::lexical_cast<T>(params[idx]);
		}
		catch(...)
		{
			throw JsonInvalidParamsException(*this);
		}
	}

	bool isParam(size_t idx, const std::string& param);

protected:
	Json json_;

};



class JsonNotification
{
public:
	static Json getJson(const std::string& method, Json data);

};



class JsonRequestException : public SnapException
{
  int errorCode_, id_;
public:
	JsonRequestException(const char* text, int errorCode = 0, int id = -1) : SnapException(text), errorCode_(errorCode), id_(id)
	{
	}

	JsonRequestException(const std::string& text, int errorCode = 0, int id = -1) : SnapException(text), errorCode_(errorCode), id_(id)
	{
	}

	JsonRequestException(const JsonRequest& request, const std::string& text, int errorCode = 0) : SnapException(text), errorCode_(errorCode), id_(request.id)
	{
	}

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


class JsonMethodNotFoundException : public JsonRequestException
{
public:
	JsonMethodNotFoundException(const JsonRequest& request) : JsonRequestException(request, "method not found", -32601)
	{
	}

	JsonMethodNotFoundException(const JsonRequest& request, const std::string& message) : JsonRequestException(request, message, -32601)
	{
	}
};



class JsonInvalidParamsException : public JsonRequestException
{
public:
	JsonInvalidParamsException(const JsonRequest& request) : JsonRequestException(request, "invalid params", -32602)
	{
	}

	JsonInvalidParamsException(const JsonRequest& request, const std::string& message) : JsonRequestException(request, message, -32602)
	{
	}
};


#endif
