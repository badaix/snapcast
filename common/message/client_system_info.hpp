/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl
    Copyright (C) 2024  Marcus Weseloh

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

#ifndef MESSAGE_CLIENT_SYSTEM_INFO_HPP
#define MESSAGE_CLIENT_SYSTEM_INFO_HPP

// local headers
#include "json_message.hpp"


namespace msg
{

// Client system information sent from client to server.
// This message can contrain arbitrary JSON data.
class ClientSystemInfo : public JsonMessage
{
public:
    ClientSystemInfo() : JsonMessage(message_type::kClientSystemInfo)
    {
    }

    ~ClientSystemInfo() override = default;
};
} // namespace msg


#endif

