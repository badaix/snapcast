/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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

#ifndef ERROR_CODE_HPP
#define ERROR_CODE_HPP


#include <optional>
#include <string>
#include <system_error>


namespace snapcast
{

struct ErrorCode : public std::error_code
{
    ErrorCode() : std::error_code(), detail_(std::nullopt)
    {
    }

    ErrorCode(const std::error_code& code) : std::error_code(code), detail_(std::nullopt)
    {
    }

    ErrorCode(const std::error_code& code, std::string detail) : std::error_code(code), detail_(std::move(detail))
    {
    }

    std::string detailed_message() const
    {
        if (detail_.has_value())
            return message() + ": " + *detail_;
        return message();
    }

private:
    std::optional<std::string> detail_;
};


} // namespace snapcast



#endif
