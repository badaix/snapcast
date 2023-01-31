/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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

#ifndef MESSAGE_FACTORY_HPP
#define MESSAGE_FACTORY_HPP

// local headers
#include "client_info.hpp"
#include "codec_header.hpp"
#include "common/str_compat.hpp"
#include "common/utils.hpp"
#include "hello.hpp"
#include "json_message.hpp"
#include "pcm_chunk.hpp"
#include "server_settings.hpp"
#include "time.hpp"

// standard headers
#include <string>


namespace msg
{

template <typename ToType>
static std::unique_ptr<ToType> message_cast(std::unique_ptr<msg::BaseMessage> message)
{
    ToType* tmp = dynamic_cast<ToType*>(message.get());
    std::unique_ptr<ToType> result;
    if (tmp != nullptr)
    {
        message.release();
        result.reset(tmp);
        return result;
    }
    return nullptr;
}

namespace factory
{

template <typename T>
static std::unique_ptr<T> createMessage(const BaseMessage& base_message, char* buffer)
{
    std::unique_ptr<T> result = std::make_unique<T>();
    if (!result)
        return nullptr;
    result->deserialize(base_message, buffer);
    return result;
}

static std::unique_ptr<BaseMessage> createMessage(const BaseMessage& base_message, char* buffer)
{
    std::unique_ptr<BaseMessage> result;
    switch (base_message.type)
    {
        case message_type::kCodecHeader:
            return createMessage<CodecHeader>(base_message, buffer);
        case message_type::kHello:
            return createMessage<Hello>(base_message, buffer);
        case message_type::kServerSettings:
            return createMessage<ServerSettings>(base_message, buffer);
        case message_type::kTime:
            return createMessage<Time>(base_message, buffer);
        case message_type::kWireChunk:
            // this is kind of cheated to safe the convertion from WireChunk to PcmChunk
            // the user of the factory must be aware that a PcmChunk will be created
            return createMessage<PcmChunk>(base_message, buffer);
        case message_type::kClientInfo:
            return createMessage<ClientInfo>(base_message, buffer);
        default:
            return nullptr;
    }
}


} // namespace factory
} // namespace msg

#endif
