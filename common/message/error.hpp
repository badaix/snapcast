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
#include "message.hpp"

namespace msg
{

/**
 * Generic error message
 */
class Error : public BaseMessage
{
public:
    /// c'tor taking the @p code, @p error and @p message of error
    explicit Error(uint32_t code, std::string error, std::string message)
        : BaseMessage(message_type::kError), code(code), error(std::move(error)), message(std::move(message))
    {
    }

    /// c'tor
    Error() : Error(0, "", "")
    {
    }

    void read(std::istream& stream) override
    {
        readVal(stream, code);
        readVal(stream, error);
        readVal(stream, message);
    }

    uint32_t getSize() const override
    {
        return static_cast<uint32_t>(sizeof(uint32_t)   // code
                                     + sizeof(uint32_t) // error string len
                                     + error.size()     // error string
                                     + sizeof(uint32_t) // message len
                                     + message.size()); // message;
    }

    /// error code
    uint32_t code;
    /// error string
    std::string error;
    /// detailed error message
    std::string message;

protected:
    void doserialize(std::ostream& stream) const override
    {
        writeVal(stream, code);
        writeVal(stream, error);
        writeVal(stream, message);
    }
};

} // namespace msg
