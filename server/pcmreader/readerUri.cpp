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


ReaderUri::ReaderUri(const std::string& readerUri)
{
// https://en.wikipedia.org/wiki/Uniform_Resource_Identifier
// scheme:[//[user:password@]host[:port]][/]path[?query][#fragment]
// would be more elegant with regex. Not yet supported on my dev machine's gcc 4.8 :(
	logE << "ReaderUri: " << readerUri << "\n";
	size_t pos;
	uri = trim_copy(readerUri);
	while (!uri.empty() && ((uri[0] == '\'') || (uri[0] == '"')))
		uri = uri.substr(1);
	while (!uri.empty() && ((uri[uri.length()-1] == '\'') || (uri[uri.length()-1] == '"')))
		uri = uri.substr(0, this->uri.length()-1);

	string decodedUri = uriDecode(uri);
	logD << "ReaderUri: " << decodedUri << "\n";

	id_ = decodedUri;
	pos = id_.find('?');
	if (pos != string::npos)
		id_ = id_.substr(0, pos);
	pos = id_.find('#');
	if (pos != string::npos)
		id_ = id_.substr(0, pos);
	logD << "id: '" << id_ << "'\n";

	string tmp(decodedUri);

	pos = tmp.find(':');
	if (pos == string::npos)
		throw invalid_argument("missing ':'");
	scheme = trim_copy(tmp.substr(0, pos));
	tmp = tmp.substr(pos + 1);
	logD << "scheme: '" << scheme << "' tmp: '" << tmp << "'\n";

	if (tmp.find("//") != 0)
		throw invalid_argument("missing host separator: '//'");
	tmp = tmp.substr(2);

	pos = tmp.find('/');
	if (pos == string::npos)
		throw invalid_argument("missing path separator: '/'");
	host = trim_copy(tmp.substr(0, pos));
	tmp = tmp.substr(pos);
	path = tmp;
	logD << "host: '" << host << "' tmp: '" << tmp << "' path: '" << path << "'\n";

	pos = tmp.find('?');
	if (pos == string::npos)
		return;

	path = trim_copy(tmp.substr(0, pos));
	tmp = tmp.substr(pos + 1);
	string queryStr = tmp;
	logD << "path: '" << path << "' tmp: '" << tmp << "' query: '" << queryStr << "'\n";

	pos = tmp.find('#');
	if (pos != string::npos)
	{
		queryStr = tmp.substr(0, pos);
		tmp = tmp.substr(pos + 1);
		fragment = trim_copy(tmp);
		logD << "query: '" << queryStr << "' fragment: '" << fragment << "' tmp: '" << tmp << "'\n";
	}

	vector<string> keyValueList = split(queryStr, '&');
	for (auto& kv: keyValueList)
	{
		pos = kv.find('=');
		if (pos != string::npos)
		{
			string key = trim_copy(kv.substr(0, pos));
			string value = trim_copy(kv.substr(pos+1));
			query[key] = value;
			if (key == "id")
				id_ = value;
		}
	}
}


json ReaderUri::toJson() const
{
	json j = {
		{"raw", uri},
		{"scheme", scheme},
		{"host", host},
		{"path", path},
		{"fragment", fragment},
		{"query", query}
	};
	return j;
}


std::string ReaderUri::id() const
{
	return id_;
}