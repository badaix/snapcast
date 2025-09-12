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
#include "authinfo.hpp"
#include "common/message/message.hpp"
#include "streamreader/stream_manager.hpp"

// 3rd party headers
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/strand.hpp>

// standard headers
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>



class StreamSession;

/// Write result callback function type
using WriteHandler = std::function<void(boost::system::error_code ec, std::size_t length)>;

/// Interface: callback for a received message.
class StreamMessageReceiver
{
public:
    /// message received callback
    virtual void onMessageReceived(const std::shared_ptr<StreamSession>& connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
    /// disonnect callback
    virtual void onDisconnect(StreamSession* connection) = 0;
};


/// A reference-counted non-modifiable buffer class.
class shared_const_buffer
{
    /// the message
    struct Message
    {
        std::vector<char> data;           ///< data
        bool is_pcm_chunk;                ///< is it a PCM chunk
        message_type type;                ///< message type
        chronos::time_point_clk rec_time; ///< recording time
    };

public:
    /// c'tor
    explicit shared_const_buffer(msg::BaseMessage& message)
    {
        tv t;
        message.sent = t;
        const msg::PcmChunk* pcm_chunk = dynamic_cast<const msg::PcmChunk*>(&message);
        message_ = std::make_shared<Message>();
        message_->type = message.type;
        message_->is_pcm_chunk = (pcm_chunk != nullptr);
        if (message_->is_pcm_chunk)
            message_->rec_time = pcm_chunk->start();

        std::ostringstream oss;
        message.serialize(oss);
        std::string s = oss.str();
        message_->data = std::vector<char>(s.begin(), s.end());
        buffer_ = boost::asio::buffer(message_->data);
    }

    /// const buffer.
    using value_type = boost::asio::const_buffer;
    /// const buffer iterator
    using const_iterator = const boost::asio::const_buffer*;

    /// begin iterator
    const boost::asio::const_buffer* begin() const
    {
        return &buffer_;
    }

    /// end iterator
    const boost::asio::const_buffer* end() const
    {
        return &buffer_ + 1;
    }

    /// the payload
    const Message& message() const
    {
        return *message_;
    }

    /// set write callback
    void setWriteHandler(WriteHandler&& handler)
    {
        handler_ = std::move(handler);
    }

    /// get write callback
    const WriteHandler& getWriteHandler() const
    {
        return handler_;
    }

    /// is the buffer sent?
    bool on_air{false};

private:
    std::shared_ptr<Message> message_;
    boost::asio::const_buffer buffer_;
    WriteHandler handler_;
};


/// Endpoint for a connected client.
/**
 * Endpoint for a connected client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the StreamMessageReceiver callback
 */
class StreamSession : public std::enable_shared_from_this<StreamSession>
{
public:
    /// c'tor. Received message from the client are passed to StreamMessageReceiver
    StreamSession(const boost::asio::any_io_executor& executor, const ServerSettings& server_settings, StreamMessageReceiver* receiver);
    /// d'tor
    virtual ~StreamSession() = default;

    /// @return the IP of the connected streaming client
    virtual std::string getIP() = 0;

    /// Start the StreamSession, e.g. start reading and processing data
    virtual void start() = 0;
    /// Stop the StreamSession
    virtual void stop() = 0;

    /// Set the message receiver to @p receiver
    void setMessageReceiver(StreamMessageReceiver* receiver)
    {
        messageReceiver_ = receiver;
    }

protected:
    /// Send data @p buffer to the streaming client, result is returned in the callback @p handler
    virtual void sendAsync(const std::shared_ptr<shared_const_buffer> buffer, WriteHandler&& handler) = 0;

public:
    /// Sends a message to the client (asynchronous)
    void send(const msg::message_ptr& message, WriteHandler&& handler = nullptr);

    /// Sends a message to the client (asynchronous)
    void send(shared_const_buffer const_buf, WriteHandler&& handler = nullptr);

    /// Max playout latency. No need to send PCM data that is older than bufferMs
    void setBufferMs(size_t bufferMs);

    /// Client id of the session
    std::string clientId;

    /// Set the sessions PCM stream
    void setPcmStream(streamreader::PcmStreamPtr pcmStream);
    /// Get the sessions PCM stream
    const streamreader::PcmStreamPtr pcmStream() const;

    /// Authentication info attached to this session
    AuthInfo authinfo;

protected:
    /// Send next message from "messages_"
    void sendNext();

    msg::BaseMessage baseMessage_;                             ///< base message buffer
    std::vector<char> buffer_;                                 ///< buffer
    size_t base_msg_size_;                                     ///< size of a base message
    StreamMessageReceiver* messageReceiver_;                   ///< message receiver
    size_t bufferMs_;                                          ///< buffer size in [ms]
    streamreader::PcmStreamPtr pcm_stream_;                    ///< the sessions PCM stream
    boost::asio::strand<boost::asio::any_io_executor> strand_; ///< strand to sync IO on
    std::deque<shared_const_buffer> messages_;                 ///< messages to be sent
    mutable std::mutex mutex_;                                 ///< protect pcm_stream_
};
