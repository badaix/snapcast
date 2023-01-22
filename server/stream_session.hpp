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

#ifndef STREAM_SESSION_HPP
#define STREAM_SESSION_HPP

// local headers
#include "common/message/message.hpp"
#include "common/queue.h"
#include "streamreader/stream_manager.hpp"

// 3rd party headers
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/strand.hpp>

// standard headers
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>



class StreamSession;


/// Interface: callback for a received message.
class StreamMessageReceiver
{
public:
    virtual void onMessageReceived(StreamSession* connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
    virtual void onDisconnect(StreamSession* connection) = 0;
};


// A reference-counted non-modifiable buffer class.
class shared_const_buffer
{
    struct Message
    {
        std::vector<char> data;
        bool is_pcm_chunk;
        message_type type;
        chronos::time_point_clk rec_time;
    };

public:
    shared_const_buffer(msg::BaseMessage& message) : on_air(false)
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

    // Implement the ConstBufferSequence requirements.
    using value_type = boost::asio::const_buffer;
    using const_iterator = const boost::asio::const_buffer*;
    const boost::asio::const_buffer* begin() const
    {
        return &buffer_;
    }

    const boost::asio::const_buffer* end() const
    {
        return &buffer_ + 1;
    }

    const Message& message() const
    {
        return *message_;
    }

    bool on_air;

private:
    std::shared_ptr<Message> message_;
    boost::asio::const_buffer buffer_;
};


using WriteHandler = std::function<void(boost::system::error_code ec, std::size_t length)>;

/// Endpoint for a connected client.
/**
 * Endpoint for a connected client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the StreamMessageReceiver callback
 */
class StreamSession : public std::enable_shared_from_this<StreamSession>
{
public:
    /// ctor. Received message from the client are passed to StreamMessageReceiver
    StreamSession(const boost::asio::any_io_executor& executor, StreamMessageReceiver* receiver);
    virtual ~StreamSession() = default;

    virtual std::string getIP() = 0;

    virtual void start() = 0;
    virtual void stop() = 0;

    void setMessageReceiver(StreamMessageReceiver* receiver)
    {
        messageReceiver_ = receiver;
    }

protected:
    virtual void sendAsync(const shared_const_buffer& buffer, const WriteHandler& handler) = 0;

public:
    /// Sends a message to the client (asynchronous)
    void send(msg::message_ptr message);

    /// Sends a message to the client (asynchronous)
    void send(shared_const_buffer const_buf);

    /// Max playout latency. No need to send PCM data that is older than bufferMs
    void setBufferMs(size_t bufferMs);

    std::string clientId;

    void setPcmStream(streamreader::PcmStreamPtr pcmStream);
    const streamreader::PcmStreamPtr pcmStream() const;

protected:
    void send_next();

    msg::BaseMessage baseMessage_;
    std::vector<char> buffer_;
    size_t base_msg_size_;
    StreamMessageReceiver* messageReceiver_;
    size_t bufferMs_;
    streamreader::PcmStreamPtr pcmStream_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    std::deque<shared_const_buffer> messages_;
    mutable std::mutex mutex_;
};



#endif
