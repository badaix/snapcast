/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#include "common/queue.h"
#include "message/message.hpp"
#include "streamreader/stream_manager.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>


using boost::asio::ip::tcp;


class StreamSession;


/// Interface: callback for a received message.
class MessageReceiver
{
public:
    virtual void onMessageReceived(StreamSession* connection, const msg::BaseMessage& baseMessage, char* buffer) = 0;
    virtual void onDisconnect(StreamSession* connection) = 0;
};


// A reference-counted non-modifiable buffer class.
// TODO: add overload for messages
class shared_const_buffer
{
public:
    // Construct from a std::string.
    explicit shared_const_buffer(const std::string& data) : data_(new std::vector<char>(data.begin(), data.end())), buffer_(boost::asio::buffer(*data_))
    {
    }

    // // Construct from a message.
    // explicit shared_const_buffer(const msg::BaseMessage& message)
    // {
    //     std::ostringstream oss;
    //     message.serialize(oss);

    //     data_ = std::shared_ptr<std::vector<char>>(new std::vector<char>(oss.str().begin(), oss.str().end()));
    //     //std::make_shared<std::vector<char>>(oss.str().begin(), oss.str().end());
    //     buffer_ = boost::asio::buffer(*data_);
    // }


    // Implement the ConstBufferSequence requirements.
    typedef boost::asio::const_buffer value_type;
    typedef const boost::asio::const_buffer* const_iterator;
    const boost::asio::const_buffer* begin() const
    {
        return &buffer_;
    }
    const boost::asio::const_buffer* end() const
    {
        return &buffer_ + 1;
    }

private:
    std::shared_ptr<std::vector<char>> data_;
    boost::asio::const_buffer buffer_;
};


/// Endpoint for a connected client.
/**
 * Endpoint for a connected client.
 * Messages are sent to the client with the "send" method.
 * Received messages from the client are passed to the MessageReceiver callback
 */
class StreamSession : public std::enable_shared_from_this<StreamSession>
{
public:
    /// ctor. Received message from the client are passed to MessageReceiver
    StreamSession(boost::asio::io_context& ioc, MessageReceiver* receiver, tcp::socket&& socket);
    ~StreamSession();
    void start();
    void stop();

    /// Sends a message to the client (asynchronous)
    void sendAsync(msg::message_ptr message, bool send_now = false);

    /// Sends a message to the client (asynchronous)
    void sendAsync(shared_const_buffer const_buf, bool send_now = false);

    /// Max playout latency. No need to send PCM data that is older than bufferMs
    void setBufferMs(size_t bufferMs);

    std::string clientId;

    std::string getIP()
    {
        return socket_.remote_endpoint().address().to_string();
    }

    void setPcmStream(streamreader::PcmStreamPtr pcmStream);
    const streamreader::PcmStreamPtr pcmStream() const;

protected:
    void read_next();
    void send_next();

    msg::BaseMessage baseMessage_;
    std::vector<char> buffer_;
    size_t base_msg_size_;
    tcp::socket socket_;
    MessageReceiver* messageReceiver_;
    size_t bufferMs_;
    streamreader::PcmStreamPtr pcmStream_;
    boost::asio::io_context::strand strand_;
    std::deque<shared_const_buffer> messages_;
};



#endif
