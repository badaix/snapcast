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
#include "common/endian.hpp"
#include "common/time_defs.hpp"

// standard headers
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <streambuf>
#ifndef WINDOWS
#include <sys/time.h>
#endif
#include <memory>

/*
template<typename CharT, typename TraitsT = std::char_traits<CharT> >
class vectorwrapbuf : public std::basic_streambuf<CharT, TraitsT>
{
public:
        vectorwrapbuf(std::vector<CharT> &vec)
        {
                this->setg(vec.data(), vec.data(), vec.data() + vec.size());
        }
};
*/

// https://stackoverflow.com/questions/7781898/get-an-istream-from-a-char
/// Helper to convert char* to a stream buffer
struct membuf : public std::basic_streambuf<char>
{
    /// c'tor
    membuf(char* begin, char* end)
    {
        this->setg(begin, begin, end);
    }
};

/// Message type enums
enum class message_type : uint16_t
{
    kBase = 0,
    kCodecHeader = 1,
    kWireChunk = 2,
    kServerSettings = 3,
    kTime = 4,
    kHello = 5,
    // kStreamTags = 6,
    kClientInfo = 7,

    kFirst = kBase,
    kLast = kClientInfo
};

/// Message type to string
static std::ostream& operator<<(std::ostream& os, const message_type& msg_type)
{
    switch (msg_type)
    {
        case message_type::kBase:
            os << "Base";
            break;
        case message_type::kCodecHeader:
            os << "CodecHeader";
            break;
        case message_type::kWireChunk:
            os << "WireChunk";
            break;
        case message_type::kServerSettings:
            os << "ServerSettings";
            break;
        case message_type::kTime:
            os << "Time";
            break;
        case message_type::kHello:
            os << "Hello";
            break;
        case message_type::kClientInfo:
            os << "ClientInfo";
            break;
        default:
            os << "Unknown";
    }
    return os;
}

/// Timeval like struct
struct tv
{
    /// c'tor
    tv()
    {
        timeval t;
        chronos::steadytimeofday(&t);
        sec = t.tv_sec;
        usec = t.tv_usec;
    }

    /// C'tor, construct from timeval @p tv
    explicit tv(timeval tv) : sec(tv.tv_sec), usec(tv.tv_usec){};
    /// C'tor, construct from @p _sec and @p _usec
    tv(int32_t _sec, int32_t _usec) : sec(_sec), usec(_usec){};

    /// seconds
    int32_t sec;
    /// micro seconds
    int32_t usec;

    /// add another tv
    tv operator+(const tv& other) const
    {
        tv result(*this);
        result.sec += other.sec;
        result.usec += other.usec;
        if (result.usec > 1000000)
        {
            result.sec += result.usec / 1000000;
            result.usec %= 1000000;
        }
        return result;
    }

    /// subtract another tv
    tv operator-(const tv& other) const
    {
        tv result(*this);
        result.sec -= other.sec;
        result.usec -= other.usec;
        while (result.usec < 0)
        {
            result.sec -= 1;
            result.usec += 1000000;
        }
        return result;
    }
};

namespace msg
{

/// Max message size = 1mb
const size_t max_size = 1000000;

struct BaseMessage;

using message_ptr = std::shared_ptr<msg::BaseMessage>;

/// Base message with common message header for all messages
struct BaseMessage
{
    /// c'tor
    BaseMessage() : BaseMessage(message_type::kBase)
    {
    }

    /// c'tor with message type @p type_
    explicit BaseMessage(message_type type_) : type(type_), id(0), refersTo(0), size(0)
    {
    }

    /// d'tor
    virtual ~BaseMessage() = default;

    /// Deserialize message from @p stream
    virtual void read(std::istream& stream)
    {
        readVal(stream, type);
        readVal(stream, id);
        readVal(stream, refersTo);
        readVal(stream, sent.sec);
        readVal(stream, sent.usec);
        readVal(stream, received.sec);
        readVal(stream, received.usec);
        readVal(stream, size);
    }

    /// Deserialize message from @p payload buffer
    void deserialize(char* payload)
    {
        membuf databuf(payload, payload + BaseMessage::getSize());
        std::istream is(&databuf);
        read(is);
    }

    /// Deserialize message from message header @p baseMessage and message @p payload
    void deserialize(const BaseMessage& baseMessage, char* payload)
    {
        type = baseMessage.type;
        id = baseMessage.id;
        refersTo = baseMessage.refersTo;
        sent = baseMessage.sent;
        received = baseMessage.received;
        size = baseMessage.size;
        membuf databuf(payload, payload + size);
        std::istream is(&databuf);
        read(is);
    }

    /// Serialize message to @p stream
    virtual void serialize(std::ostream& stream) const
    {
        writeVal(stream, type);
        writeVal(stream, id);
        writeVal(stream, refersTo);
        writeVal(stream, sent.sec);
        writeVal(stream, sent.usec);
        writeVal(stream, received.sec);
        writeVal(stream, received.usec);
        size = getSize();
        writeVal(stream, size);
        doserialize(stream);
    }

    /// @return binary size of the base message
    virtual uint32_t getSize() const
    {
        return 3 * sizeof(uint16_t) + 2 * sizeof(tv) + sizeof(uint32_t);
    };

    /// the message type
    message_type type;
    /// the message id, used to correlate request and response
    mutable uint16_t id;
    /// a response message might refer to the message id of the request
    uint16_t refersTo;
    /// timestamp when the message has been received
    tv received;
    /// timestamp when the message was sent
    mutable tv sent;
    /// the payload size
    mutable uint32_t size;

protected:
    /// serialize a bool @p val to @p stream
    void writeVal(std::ostream& stream, const bool& val) const
    {
        char c = val ? 1 : 0;
        writeVal(stream, c);
    }

    /// serialize a const char* @p val to @p stream
    void writeVal(std::ostream& stream, const char& val) const
    {
        stream.write(reinterpret_cast<const char*>(&val), sizeof(char));
    }

    /// serialize an uint16 @p val to @p stream
    void writeVal(std::ostream& stream, const uint16_t& val) const
    {
        uint16_t v = SWAP_16(val);
        stream.write(reinterpret_cast<const char*>(&v), sizeof(uint16_t));
    }

    /// serialize the message type @p val to @p stream
    void writeVal(std::ostream& stream, const message_type& val) const
    {
        uint16_t v = SWAP_16(static_cast<uint16_t>(val));
        stream.write(reinterpret_cast<const char*>(&v), sizeof(uint16_t));
    }

    /// serialize an int16 @p val to @p stream
    void writeVal(std::ostream& stream, const int16_t& val) const
    {
        uint16_t v = SWAP_16(val);
        stream.write(reinterpret_cast<const char*>(&v), sizeof(int16_t));
    }

    /// serialize an uint32 @p val to @p stream
    void writeVal(std::ostream& stream, const uint32_t& val) const
    {
        uint32_t v = SWAP_32(val);
        stream.write(reinterpret_cast<const char*>(&v), sizeof(uint32_t));
    }

    /// serialize an int32 @p val to @p stream
    void writeVal(std::ostream& stream, const int32_t& val) const
    {
        uint32_t v = SWAP_32(val);
        stream.write(reinterpret_cast<const char*>(&v), sizeof(int32_t));
    }

    /// serialize a char* string @p payload of @p size to @p stream
    void writeVal(std::ostream& stream, const char* payload, const uint32_t& size) const
    {
        writeVal(stream, size);
        stream.write(payload, size);
    }

    /// serialize an std::string @p val to @p stream
    void writeVal(std::ostream& stream, const std::string& val) const
    {
        auto len = static_cast<uint32_t>(val.size());
        writeVal(stream, val.c_str(), len);
    }


    /// Deserialize a bool @p val from @p stream
    void readVal(std::istream& stream, bool& val) const
    {
        char c;
        readVal(stream, c);
        val = (c != 0);
    }

    /// Deserialize a char @p val from @p stream
    void readVal(std::istream& stream, char& val) const
    {
        stream.read(reinterpret_cast<char*>(&val), sizeof(char));
    }

    /// Deserialize an uint16 @p val from @p stream
    void readVal(std::istream& stream, uint16_t& val) const
    {
        stream.read(reinterpret_cast<char*>(&val), sizeof(uint16_t));
        // cppcheck-suppress selfAssignment
        val = SWAP_16(val);
    }

    /// Deserialize a message_type @p val from @p stream
    void readVal(std::istream& stream, message_type& val) const
    {
        stream.read(reinterpret_cast<char*>(&val), sizeof(uint16_t));
        val = static_cast<message_type>(SWAP_16(static_cast<uint16_t>(val)));
    }

    /// Deserialize an int16 @p val from @p stream
    void readVal(std::istream& stream, int16_t& val) const
    {
        stream.read(reinterpret_cast<char*>(&val), sizeof(int16_t));
        // cppcheck-suppress selfAssignment
        val = SWAP_16(val);
    }

    /// Deserialize an uint32 @p val from @p stream
    void readVal(std::istream& stream, uint32_t& val) const
    {
        stream.read(reinterpret_cast<char*>(&val), sizeof(uint32_t));
        // cppcheck-suppress selfAssignment
        val = SWAP_32(val);
    }

    /// Deserialize an int32 @p val from @p stream
    void readVal(std::istream& stream, int32_t& val) const
    {
        stream.read(reinterpret_cast<char*>(&val), sizeof(int32_t));
        // cppcheck-suppress selfAssignment
        val = SWAP_32(val);
    }

    /// Deserialize a char* string @p payload of @p size from @p stream
    void readVal(std::istream& stream, char** payload, uint32_t& size) const
    {
        readVal(stream, size);
        *payload = static_cast<char*>(realloc(*payload, size));
        stream.read(*payload, size);
    }

    /// Deserialize a std::string @p val from @p stream
    void readVal(std::istream& stream, std::string& val) const
    {
        uint32_t len;
        readVal(stream, len);
        val.resize(len);
        stream.read(&val[0], len);
    }

    /// Deserialize message to std::ostream
    virtual void doserialize(std::ostream& /*stream*/) const {};
};

} // namespace msg
