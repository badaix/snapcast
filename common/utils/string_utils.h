/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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

#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stdio.h>
#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

namespace utils
{
namespace string
{

// trim from start
static inline std::string &ltrim(std::string &s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

// trim from end
static inline std::string &rtrim(std::string &s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

// trim from both ends
static inline std::string &trim(std::string &s)
{
	return ltrim(rtrim(s));
}

// trim from start
static inline std::string ltrim_copy(const std::string &s)
{
	std::string str(s);
	return ltrim(str);
}

// trim from end
static inline std::string rtrim_copy(const std::string &s)
{
	std::string str(s);
	return rtrim(str);
}

// trim from both ends
static inline std::string trim_copy(const std::string &s)
{
	std::string str(s);
	return trim(str);
}

// decode %xx to char
static std::string uriDecode(const std::string& src) {
	std::string ret;
	char ch;
	for (size_t i=0; i<src.length(); i++)
	{
		if (int(src[i]) == 37)
		{
			unsigned int ii;
			sscanf(src.substr(i+1, 2).c_str(), "%x", &ii);
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



static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim))
	{
		elems.push_back(item);
	}
	return elems;
}


static std::vector<std::string> split(const std::string &s, char delim)
{
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}

} // namespace string
} // namespace utils

#endif

