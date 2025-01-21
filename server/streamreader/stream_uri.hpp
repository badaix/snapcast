/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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

#pragma once


// local headers
#include "common/json.hpp"

// standard headers
#include <map>
#include <string>


using json = nlohmann::json;

namespace streamreader
{

/// URI with the general format:
///  scheme:[//[user:password@]host[:port]][/]path[?query][#fragment]
struct StreamUri
{
    /// c'tor construct from string @p uri
    explicit StreamUri(const std::string& uri);

    /// the complete uri
    std::string uri;
    /// the scheme component (pipe, http, file, tcp, ...)
    std::string scheme;
    // struct Authority
    // {
    //     std::string username;
    //     std::string password;
    //     std::string host;
    //     size_t port;
    // };
    // Authority authority;

    /// the host component
    std::string host;
    /// the path component
    std::string path;
    /// the query component: "key = value" pairs
    std::map<std::string, std::string> query;
    /// the fragment component
    std::string fragment;

    /// @return URI as json
    json toJson() const;

    /// @return value for a @p key or @p def, if key does not exist
    std::string getQuery(const std::string& key, const std::string& def = "") const;

    /// parse @p stream_uri string
    void parse(const std::string& stream_uri);

    /// @return uri as string
    std::string toString() const;

    /// @return true if @p other is equal to this
    bool operator==(const StreamUri& other) const;
};

} // namespace streamreader
