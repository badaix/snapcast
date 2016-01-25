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


#include <common/utils.h>
#include <common/compat.h>
#include <common/log.h>
#include "readerUri.h"


using namespace std;


ReaderUri::ReaderUri(const std::string& uri)
{
// https://en.wikipedia.org/wiki/Uniform_Resource_Identifier
// scheme:[//[user:password@]host[:port]][/]path[?query][#fragment]
// would be more elegant with regex. Not yet supported on my dev machine's gcc 4.8 :(
	size_t pos;
	this->uri = uri;
	string tmp(uri);

	pos = tmp.find(':');
	if (pos == string::npos)
		throw invalid_argument("missing ':'");
	scheme = tmp.substr(0, pos);
	tmp = tmp.substr(pos + 1);
	logE << "scheme: '" << scheme << "' tmp: '" << tmp << "'\n";

	if (tmp.find("//") != 0)
		throw invalid_argument("missing host separator: '//'");
	tmp = tmp.substr(2);

	pos = tmp.find('/');
	if (pos == string::npos)
		throw invalid_argument("missing path separator: '/'");
	host = tmp.substr(0, pos);
	tmp = tmp.substr(pos);
	path = tmp;
	logE << "host: '" << host << "' tmp: '" << tmp << "' path: '" << path << "'\n";

	pos = tmp.find('?');
	if (pos == string::npos)
		return;

	path = tmp.substr(0, pos);
	tmp = tmp.substr(pos + 1);
	string queryStr = tmp;
	logE << "path: '" << path << "' tmp: '" << tmp << "' query: '" << queryStr << "'\n";

	pos = tmp.find('#');
	if (pos != string::npos)
	{
		queryStr = tmp.substr(0, pos);
		tmp = tmp.substr(pos + 1);
		fragment = tmp;
		logE << "query: '" << queryStr << "' fragment: '" << fragment << "' tmp: '" << tmp << "'\n";
	}

	vector<string> keyValueList = split(queryStr, '&');
	for (auto& kv: keyValueList)
	{
		pos = kv.find('=');
		if (pos != string::npos)
			query[kv.substr(0, pos)] = kv.substr(pos+1);
	}
}


json ReaderUri::toJson() const
{
	json j;
	j["uri"] = uri;
	j["scheme"] = scheme;
	j["host"] = host;
	j["path"] = path;
	j["fragment"] = fragment;
	j["query"] = json(query);
	return j;
}

