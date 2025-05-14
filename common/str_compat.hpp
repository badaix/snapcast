/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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
#include <clocale>
#include <string>

#ifdef NO_CPP11_STRING
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#endif


namespace cpt
{
static struct lconv* localeconv()
{
#ifdef NO_CPP11_STRING
    static struct lconv result;
    result.decimal_point = nullptr;
    result.thousands_sep = nullptr;
    return &result;
#else
    return std::localeconv();
#endif
}

template <typename T>
static std::string to_string(const T& t)
{
#ifdef NO_CPP11_STRING
    std::stringstream ss;
    ss << t;
    return ss.str();
#else
    return std::to_string(t);
#endif
}

static long stoul(const std::string& str)
{
#ifdef NO_CPP11_STRING
    errno = 0;
    char* temp;
    long val = strtol(str.c_str(), &temp, 10);
    if (temp == str.c_str() || *temp != '\0')
        throw std::invalid_argument("stoi");
    if (((val == LONG_MIN) || (val == LONG_MAX)) && (errno == ERANGE))
        throw std::out_of_range("stoi");
    return val;
#else
    return std::stoul(str);
#endif
}

static int stoi(const std::string& str)
{
#ifdef NO_CPP11_STRING
    return cpt::stoul(str);
#else
    return std::stoi(str);
#endif
}

static int stoi(const std::string& str, int def)
{
    try
    {
        return cpt::stoi(str);
    }
    catch (...)
    {
        return def;
    }
}

static double stod(const std::string& str)
{
#ifdef NO_CPP11_STRING
    errno = 0;
    char* temp;
    double val = strtod(str.c_str(), &temp);
    if (temp == str.c_str() || *temp != '\0')
        throw std::invalid_argument("stod");
    if ((val == HUGE_VAL) && (errno == ERANGE))
        throw std::out_of_range("stod");
    return val;
#else
    return std::stod(str.c_str());
#endif
}

static long double strtold(const char* str, char** endptr)
{
#ifdef NO_CPP11_STRING
    std::ignore = endptr;
    return cpt::stod(str);
#else
    return std::strtold(str, endptr);
#endif
}

static float strtof(const char* str, char** endptr)
{
#ifdef NO_CPP11_STRING
    std::ignore = endptr;
    return (float)cpt::stod(str);
#else
    return std::strtof(str, endptr);
#endif
}
} // namespace cpt
