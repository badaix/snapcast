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
#include "jsonrpcException.h"


using Json = nlohmann::json;



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
	std::map<std::string, Json> params;

	Json getResponse(const Json& result);
	Json getError(int code, const std::string& message);

	Json getParam(const std::string& key);
	bool hasParam(const std::string& key);

	template<typename T>
	T getParam(const std::string& key, const T& lowerRange, const T& upperRange)
	{
		T value = getParam(key).get<T>();
		if (value < lowerRange)
			throw JsonInvalidParamsException(key + " out of range", id);
		else if (value > upperRange)
			throw JsonInvalidParamsException(key + " out of range", id);

		return value;
	}


protected:
	Json json_;

};



class JsonNotification
{
public:
	static Json getJson(const std::string& method, Json data);

};





#endif
