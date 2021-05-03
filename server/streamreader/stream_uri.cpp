/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include "stream_uri.hpp"
#include "common/aixlog.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"


using namespace std;
namespace strutils = utils::string;

namespace streamreader
{

StreamUri::StreamUri(const std::string& uri)
{
    parse(uri);
}


void StreamUri::parse(const std::string& streamUri)
{
    // https://en.wikipedia.org/wiki/Uniform_Resource_Identifier
    // scheme:[//[user:password@]host[:port]][/]path[?query][#fragment]
    // would be more elegant with regex. Not yet supported on my dev machine's gcc 4.8 :(
    LOG(DEBUG) << "StreamUri: " << streamUri << "\n";
    size_t pos;
    uri = strutils::trim_copy(streamUri);
    while (!uri.empty() && ((uri[0] == '\'') || (uri[0] == '"')))
        uri = uri.substr(1);
    while (!uri.empty() && ((uri[uri.length() - 1] == '\'') || (uri[uri.length() - 1] == '"')))
        uri = uri.substr(0, this->uri.length() - 1);

    // string decodedUri = strutils::uriDecode(uri);
    // LOG(DEBUG) << "StreamUri decoded: " << decodedUri << "\n";

    string tmp(uri);

    pos = tmp.find(':');
    if (pos == string::npos)
        throw invalid_argument("missing ':'");
    scheme = strutils::uriDecode(strutils::trim_copy(tmp.substr(0, pos)));
    tmp = tmp.substr(pos + 1);
    LOG(TRACE) << "scheme: '" << scheme << "', tmp: '" << tmp << "'\n";

    if (tmp.find("//") != 0)
        throw invalid_argument("missing host separator: '//'");
    tmp = tmp.substr(2);

    pos = tmp.find('/');
    if (pos == string::npos)
    {
        pos = tmp.find('?');
        if (pos == string::npos)
            pos = tmp.length();
    }

    host = strutils::uriDecode(strutils::trim_copy(tmp.substr(0, pos)));
    tmp = tmp.substr(pos);
    path = tmp;
    pos = std::min(path.find('?'), path.find('#'));
    path = strutils::uriDecode(strutils::trim_copy(path.substr(0, pos)));
    LOG(TRACE) << "host: '" << host << "', tmp: '" << tmp << "', path: '" << path << "'\n";

    string queryStr;
    pos = tmp.find('?');
    if (pos != string::npos)
    {
        tmp = tmp.substr(pos + 1);
        queryStr = tmp;
        LOG(TRACE) << "path: '" << path << "', tmp: '" << tmp << "', query: '" << queryStr << "'\n";
    }

    pos = tmp.find('#');
    if (pos != string::npos)
    {
        queryStr = tmp.substr(0, pos);
        tmp = tmp.substr(pos + 1);
        fragment = strutils::uriDecode(strutils::trim_copy(tmp));
        LOG(TRACE) << "query: '" << queryStr << "', fragment: '" << fragment << "', tmp: '" << tmp << "'\n";
    }

    vector<string> keyValueList = strutils::split(queryStr, '&');
    for (auto& kv : keyValueList)
    {
        pos = kv.find('=');
        if (pos != string::npos)
        {
            string key = strutils::uriDecode(strutils::trim_copy(kv.substr(0, pos)));
            string value = strutils::uriDecode(strutils::trim_copy(kv.substr(pos + 1)));
            query[key] = value;
        }
    }
    LOG(DEBUG) << "StreamUri.toString: " << toString() << "\n";
}


std::string StreamUri::toString() const
{
    // scheme:[//[user:password@]host[:port]][/]path[?query][#fragment]
    stringstream ss;
    ss << scheme << "://" << host << "/" + path;
    if (!query.empty())
    {
        ss << "?";
        auto iter = query.begin();
        while (true)
        {
            ss << iter->first << "=" << iter->second;
            if (++iter == query.end())
                break;
            ss << "&";
        }
    }
    if (!fragment.empty())
        ss << "#" << fragment;

    return ss.str();
}


json StreamUri::toJson() const
{
    json j = {{"raw", toString()}, {"scheme", scheme}, {"host", host}, {"path", path}, {"fragment", fragment}, {"query", query}};
    return j;
}


std::string StreamUri::getQuery(const std::string& key, const std::string& def) const
{
    auto iter = query.find(key);
    if (iter != query.end())
        return iter->second;
    return def;
}
} // namespace streamreader
