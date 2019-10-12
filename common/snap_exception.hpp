/***
    This file is part of snapcast
    Copyright (C) 2014-2019  Johannes Pohl

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

#ifndef SNAP_EXCEPTION_H
#define SNAP_EXCEPTION_H

#include <cstring> // std::strlen, std::strcpy
#include <exception>
#include <string>

// text_exception uses a dynamically-allocated internal c-string for what():
class SnapException : public std::exception
{
    char* text_;

public:
    SnapException(const char* text)
    {
        text_ = new char[std::strlen(text) + 1];
        std::strcpy(text_, text);
    }

    SnapException(const std::string& text) : SnapException(text.c_str())
    {
    }

    SnapException(const SnapException& e) : SnapException(e.what())
    {
    }

    ~SnapException() throw() override
    {
        delete[] text_;
    }

    const char* what() const noexcept override
    {
        return text_;
    }
};



class AsyncSnapException : public SnapException
{
public:
    AsyncSnapException(const char* text) : SnapException(text)
    {
    }

    AsyncSnapException(const std::string& text) : SnapException(text)
    {
    }

    AsyncSnapException(const AsyncSnapException& e) : SnapException(e.what())
    {
    }


    ~AsyncSnapException() throw() override = default;
};



#endif
