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
#include <exception>
#include <string>

/// Snapcast specific exceptions
class SnapException : public std::exception
{
    std::string text_;
    int error_code_;

public:
    /// c'tor
    explicit SnapException(const char* text, int error_code = 0) : text_(text), error_code_(error_code)
    {
    }

    /// c'tor
    explicit SnapException(const std::string& text, int error_code = 0) : SnapException(text.c_str(), error_code)
    {
    }

    /// d'tor
    ~SnapException() override = default;

    /// @return error code
    int code() const noexcept
    {
        return error_code_;
    }

    /// @return the exception text
    const char* what() const noexcept override
    {
        return text_.c_str();
    }
};
