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

#ifndef SNAP_EXCEPTION_HPP
#define SNAP_EXCEPTION_HPP

#include <cstring> // std::strlen, std::strcpy
#include <exception>
#include <string>

// text_exception uses a dynamically-allocated internal c-string for what():
class SnapException : public std::exception
{
    std::string text_;
    int error_code_;

public:
    SnapException(const char* text, int error_code = 0) : text_(text), error_code_(error_code)
    {
    }

    SnapException(const std::string& text, int error_code = 0) : SnapException(text.c_str(), error_code)
    {
    }

    ~SnapException() throw() override = default;

    int code() const noexcept
    {
        return error_code_;
    }

    const char* what() const noexcept override
    {
        return text_.c_str();
    }
};


#endif
