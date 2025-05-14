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


// standard headers
#include <map>
#include <sstream>
#include <string>
#include <vector>


namespace utils::string
{

/// Check of text matches pattern, with pattern containing '*' wildcards
bool wildcardMatch(const std::string& pattern, const std::string& text);

/// trim from start
std::string& ltrim(std::string& s);

/// trim from end
std::string& rtrim(std::string& s);

/// trim from both ends
std::string& trim(std::string& s);

/// trim from start
std::string ltrim_copy(const std::string& s);

/// trim from end
std::string rtrim_copy(const std::string& s);

/// trim from both ends
std::string trim_copy(const std::string& s);

/// decode %xx to char
std::string uriDecode(const std::string& src);

/// @return uri encoded version of @p str
std::string urlEncode(const std::string& str);

/// Split string @p s at @p delim into @p left and @p right
void split_left(const std::string& s, char delim, std::string& left, std::string& right);

/// Split string @p s at @p delim and left and @p right
/// @return the left part
std::string split_left(const std::string& s, char delim, std::string& right);

/// Split string @p s at @p delim and return the splitted list in @p elems
/// @return list of splitted strings
std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems);

/// @return resulting list of strings by splitting @p s at @p delim
std::vector<std::string> split(const std::string& s, char delim);

/// Split @p s into key value pairs, separated by @p pair_delim and @p key_value_delim
/// @return map from keys to values
template <typename T>
std::map<std::string, T> split_pairs_to_container(const std::string& s, char pair_delim, char key_value_delim)
{
    std::map<std::string, T> result;
    auto keyValueList = split(s, pair_delim);
    for (const auto& kv : keyValueList)
    {
        auto pos = kv.find(key_value_delim);
        if (pos != std::string::npos)
        {
            std::string key = trim_copy(kv.substr(0, pos));
            std::string value = trim_copy(kv.substr(pos + 1));
            result[key].push_back(std::move(value));
        }
    }
    return result;
}

/// @return concatenated values of @p container, separated by @p delim
template <typename T>
std::string container_to_string(const T& container, const std::string& delim = ", ")
{
    std::stringstream ss;
    for (auto iter = container.begin(); iter != container.end(); ++iter)
    {
        ss << *iter;
        if (std::distance(iter, container.end()) > 1)
            ss << delim;
    }
    return ss.str();
}


/// Split @p s into key value pairs, separated by @p pair_delim and @p key_value_delim
/// @return map from keys to values
std::map<std::string, std::string> split_pairs(const std::string& s, char pair_delim, char key_value_delim);

/// @return @p[in, out] s converted to lowercase
std::string& tolower(std::string& s);

/// @return @p s converted to lowercase
std::string tolower_copy(const std::string& s);


} // namespace utils::string
