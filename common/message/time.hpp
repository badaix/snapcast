/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef MESSAGE_TIME_HPP
#define MESSAGE_TIME_HPP

// local headers
#include "message.hpp"

namespace msg
{

class Time : public BaseMessage
{
public:
    Time() : BaseMessage(message_type::kTime)
    {
    }

    ~Time() override = default;

    void read(std::istream& stream) override
    {
        readVal(stream, latency.sec);
        readVal(stream, latency.usec);
    }

    uint32_t getSize() const override
    {
        return sizeof(tv);
    }

    tv latency;

protected:
    void doserialize(std::ostream& stream) const override
    {
        writeVal(stream, latency.sec);
        writeVal(stream, latency.usec);
    }
};
} // namespace msg


#endif
