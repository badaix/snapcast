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


// prototype/interface header file
#include "string_utils.hpp"

// local headers
#include "common/aixlog.hpp"

// 3rd party headers

// standard headers
#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#ifdef WINDOWS
#include <cctype>
#endif

namespace utils::string
{

static constexpr auto LOG_TAG = "StringUtils";

bool wildcardMatch(const std::string& pattern, const std::string& text)
{
    LOG(INFO, LOG_TAG) << "wildcardMatch '" << pattern << "', text: '" << text << "'\n";
    std::vector<std::string> parts = utils::string::split(pattern, '*');
    size_t pos = 0;
    for (size_t n = 0; n < parts.size(); ++n)
    {
        const std::string& part = parts[n];
        LOG(INFO, LOG_TAG) << "Matching '" << part << "', pos: " << pos << "\n";
        pos = text.find(part, pos);
        if (pos == std::string::npos)
            return false;
        // if pattern does not start with "*" the first match must be the start of <test>
        if ((n == 0) && !pattern.empty() && (pattern.front() != '*') && (pos != 0))
            return false;
        // if pattern does not end with "*" the last match must end with the end of <test>
        if ((n == parts.size() - 1) && !pattern.empty() && (pattern.back() != '*') && (pos != text.size() - parts.back().size()))
            return false;
    }
    return true;
}


// trim from start
std::string& ltrim(std::string& s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
    return s;
}

// trim from end
std::string& rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
    return s;
}

// trim from both ends
std::string& trim(std::string& s)
{
    return ltrim(rtrim(s));
}

// trim from start
std::string ltrim_copy(const std::string& s)
{
    std::string str(s);
    return ltrim(str);
}

// trim from end
std::string rtrim_copy(const std::string& s)
{
    std::string str(s);
    return rtrim(str);
}

// trim from both ends
std::string trim_copy(const std::string& s)
{
    std::string str(s);
    return trim(str);
}


std::string urlEncode(const std::string& str)
{
    std::ostringstream os;
    for (std::string::const_iterator ci = str.begin(); ci != str.end(); ++ci)
    {
        if ((*ci >= 'a' && *ci <= 'z') || (*ci >= 'A' && *ci <= 'Z') || (*ci >= '0' && *ci <= '9'))
        { // allowed
            os << *ci;
        }
        else if (*ci == ' ')
        {
            os << '+';
        }
        else
        {
            auto toHex = [](unsigned char x) { return static_cast<unsigned char>(x + (x > 9 ? ('A' - 10) : '0')); };
            os << '%' << toHex(*ci >> 4) << toHex(*ci % 16);
        }
    }

    return os.str();
}


// decode %xx to char
std::string uriDecode(const std::string& src)
{
    std::string ret;
    char ch;
    for (size_t i = 0; i < src.length(); i++)
    {
        if (int(src[i]) == 37)
        {
            unsigned int ii;
            sscanf(src.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i += 2;
        }
        else
        {
            ret += src[i];
        }
    }
    return (ret);
}


void split_left(const std::string& s, char delim, std::string& left, std::string& right)
{
    auto pos = s.find(delim);
    if (pos != std::string::npos)
    {
        left = s.substr(0, pos);
        right = s.substr(pos + 1);
    }
    else
    {
        left = s;
        right = "";
    }
}


std::string split_left(const std::string& s, char delim, std::string& right)
{
    std::string left;
    split_left(s, delim, left, right);
    return left;
}



std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems)
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }

    if (!s.empty() && (s.back() == delim))
        elems.emplace_back("");

    return elems;
}


std::vector<std::string> split(const std::string& s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}


std::map<std::string, std::string> split_pairs(const std::string& s, char pair_delim, char key_value_delim)
{
    std::map<std::string, std::string> result;
    auto pairs = split_pairs_to_container<std::vector<std::string>>(s, pair_delim, key_value_delim);
    for (auto& pair : pairs)
        result[pair.first] = *pair.second.begin();
    return result;
}


std::string& tolower(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}


std::string tolower_copy(const std::string& s)
{
    std::string str(s);
    return tolower(str);
}

} // namespace utils::string
