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

#ifndef MESSAGE_JSON_HPP
#define MESSAGE_JSON_HPP

// local headers
#include "common/json.hpp"
#include "message.hpp"


using json = nlohmann::json;


namespace msg
{

class JsonMessage : public BaseMessage
{
public:
    JsonMessage(message_type msgType) : BaseMessage(msgType)
    {
    }

    ~JsonMessage() override = default;

    void read(std::istream& stream) override
    {
        std::string s;
        readVal(stream, s);
        msg = json::parse(s);
    }

    uint32_t getSize() const override
    {
        return static_cast<uint32_t>(sizeof(uint32_t) + msg.dump().size());
    }

    json msg;


protected:
    void doserialize(std::ostream& stream) const override
    {
        writeVal(stream, msg.dump());
    }

    template <typename T>
    T get(const std::string& what, const T& def) const
    {
        try
        {
            if (!msg.count(what))
                return def;
            return msg[what].get<T>();
        }
        catch (...)
        {
            return def;
        }
    }
};
} // namespace msg


#endif
