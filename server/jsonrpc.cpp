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

#include "jsonrpc.h"
#include "common/log.h"


using namespace std;



JsonRequest::JsonRequest() : id(-1), method("")
{
}


void JsonRequest::parse(const std::string& json)
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
		try
		{
			json_ = Json::parse(json);
		}
		catch (const exception& e)
		{
			throw JsonRequestException(e.what(), -32700);
		}

		id = json_["id"].get<int>();
		string jsonrpc = json_["jsonrpc"].get<string>();
		if (jsonrpc != "2.0")
			throw JsonRequestException("invalid jsonrpc value: " + jsonrpc, -32600);
		method = json_["method"].get<string>();
		if (method.empty())
			throw JsonRequestException("method must not be empty", -32600);
		if (id < 0)
			throw JsonRequestException("id must be a positive integer", -32600);

		params.clear();
		try
		{
			if (json_["params"] != nullptr)
				params = json_["params"].get<vector<string>>();
		}
		catch (const exception& e)
		{
			throw JsonRequestException(e.what(), -32602);
		}
	}
	catch (const JsonRequestException& e)
	{
		throw;
	}
	catch (const exception& e)
	{
		throw JsonRequestException(e.what(), -32603, id);
	}
}


Json JsonRequest::getResponse(const Json& result)
{
	Json response = {
		{"jsonrpc", "2.0"},
		{"id", id},
		{"result", result}
	};

	return response;
}


Json JsonRequest::getError(int code, const std::string& message)
{
	Json response = {
		{"jsonrpc", "2.0"},
		{"error", {
			{"code", code},
			{"message", message}
		}},
	};

	return response;

}



/*

			if ((method == "get") || (method == "set"))
			{
				vector<string> params;
				try
				{
					params = request["params"].get<vector<string>>();
				}
				catch (const exception& e)
				{
					throw JsonRpcException(e.what(), -32602);
				}
				if (method == "get")
				{
					//{"jsonrpc": "2.0", "method": "get", "params": ["status"], "id": 2}
					//{"jsonrpc": "2.0", "method": "get", "params": ["status", "server"], "id": 2}
					//{"jsonrpc": "2.0", "method": "get", "params": ["status", "client"], "id": 2}
					//{"jsonrpc": "2.0", "method": "get", "params": ["status", "client", "MAC"], "id": 2}
					vector<string> params = request["params"].get<vector<string>>();
					for (auto s: params)
						logO << s << "\n";
					response["result"] = "???";//nullptr;
				}
				else if (method == "set")
				{
					//{"jsonrpc": "2.0", "method": "set", "params": ["volume", "0.9", "client", "MAC"], "id": 2}
					//{"jsonrpc": "2.0", "method": "set", "params": ["active", "client", "MAC"], "id": 2}
					response["result"] = "234";//nullptr;
				}
			}
			else
				throw JsonRpcException("method not found: \"" + method + "\"", -32601);

			connection->send(response.dump());

*/



/*
		Json response = {
			{"jsonrpc", "2.0"},
			{"id", id}
		};

*/


