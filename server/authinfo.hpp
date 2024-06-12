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

// local headers
#include "common/jwt.hpp"

// 3rd party headers

// standard headers
#include <chrono>
#include <optional>
#include <string>



class AuthInfo
{
public:
    AuthInfo(std::string authheader);
    virtual ~AuthInfo() = default;

    bool valid() const;
    const std::string& username() const;

private:
    std::string username_;
    std::optional<std::chrono::system_clock::time_point> expires_;
};
