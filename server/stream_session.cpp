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

#include "stream_session.hpp"

#include "common/aixlog.hpp"
#include "message/pcm_chunk.hpp"
#include <iostream>

using namespace std;
using namespace streamreader;


static constexpr auto LOG_TAG = "StreamSession";


StreamSession::StreamSession(boost::asio::io_context& ioc, MessageReceiver* receiver, tcp::socket&& socket)
    : socket_(std::move(socket)), messageReceiver_(receiver), pcmStream_(nullptr), strand_(ioc)
{
    base_msg_size_ = baseMessage_.getSize();
    buffer_.resize(base_msg_size_);
}


StreamSession::~StreamSession()
{
    stop();
}


void StreamSession::read_next()
{
    boost::asio::async_read(socket_, boost::asio::buffer(buffer_, base_msg_size_),
                            boost::asio::bind_executor(strand_, [ this, self = shared_from_this() ](boost::system::error_code ec, std::size_t length) mutable {
                                if (ec)
                                {
                                    LOG(ERROR, LOG_TAG) << "Error reading message header of length " << length << ": " << ec.message() << "\n";
                                    messageReceiver_->onDisconnect(this);
                                    return;
                                }

                                baseMessage_.deserialize(buffer_.data());
                                LOG(DEBUG, LOG_TAG) << "getNextMessage: " << baseMessage_.type << ", size: " << baseMessage_.size << ", id: " << baseMessage_.id
                                                    << ", refers: " << baseMessage_.refersTo << "\n";
                                if (baseMessage_.type > message_type::kLast)
                                {
                                    LOG(ERROR, LOG_TAG) << "unknown message type received: " << baseMessage_.type << ", size: " << baseMessage_.size << "\n";
                                    messageReceiver_->onDisconnect(this);
                                    return;
                                }
                                else if (baseMessage_.size > msg::max_size)
                                {
                                    LOG(ERROR, LOG_TAG) << "received message of type " << baseMessage_.type << " to large: " << baseMessage_.size << "\n";
                                    messageReceiver_->onDisconnect(this);
                                    return;
                                }

                                if (baseMessage_.size > buffer_.size())
                                    buffer_.resize(baseMessage_.size);

                                boost::asio::async_read(
                                    socket_, boost::asio::buffer(buffer_, baseMessage_.size),
                                    boost::asio::bind_executor(strand_, [this, self](boost::system::error_code ec, std::size_t length) mutable {
                                        if (ec)
                                        {
                                            LOG(ERROR, LOG_TAG) << "Error reading message body of length " << length << ": " << ec.message() << "\n";
                                            messageReceiver_->onDisconnect(this);
                                            return;
                                        }

                                        tv t;
                                        baseMessage_.received = t;
                                        if (messageReceiver_ != nullptr)
                                            messageReceiver_->onMessageReceived(this, baseMessage_, buffer_.data());
                                        read_next();
                                    }));
                            }));
}


void StreamSession::setPcmStream(PcmStreamPtr pcmStream)
{
    pcmStream_ = pcmStream;
}


const PcmStreamPtr StreamSession::pcmStream() const
{
    return pcmStream_;
}


void StreamSession::start()
{
    read_next();
    // strand_.post([this]() { read_next(); });
}


void StreamSession::stop()
{
    LOG(DEBUG, LOG_TAG) << "StreamSession::stop\n";
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
        LOG(ERROR, LOG_TAG) << "Error in socket shutdown: " << ec.message() << "\n";
    socket_.close(ec);
    if (ec)
        LOG(ERROR, LOG_TAG) << "Error in socket close: " << ec.message() << "\n";
    LOG(DEBUG, LOG_TAG) << "StreamSession stopped\n";
}


void StreamSession::send_next()
{
    auto buffer = messages_.front();
    boost::asio::async_write(socket_, buffer,
                             boost::asio::bind_executor(strand_, [ this, self = shared_from_this(), buffer ](boost::system::error_code ec, std::size_t length) {
                                 messages_.pop_front();
                                 if (ec)
                                 {
                                     LOG(ERROR, LOG_TAG) << "StreamSession write error (msg length: " << length << "): " << ec.message() << "\n";
                                     messageReceiver_->onDisconnect(this);
                                     return;
                                 }
                                 if (!messages_.empty())
                                     send_next();
                             }));
}


void StreamSession::sendAsync(shared_const_buffer const_buf, bool send_now)
{
    strand_.post([ this, self = shared_from_this(), const_buf, send_now ]() {
        // delete PCM chunks that are older than the overall buffer duration
        messages_.erase(std::remove_if(messages_.begin(), messages_.end(),
                                       [this](const shared_const_buffer& buffer) {
                                           const auto& msg = buffer.message();
                                           if (!msg.is_pcm_chunk)
                                               return false;
                                           auto age = chronos::clk::now() - msg.rec_time;
                                           return (age > std::chrono::milliseconds(bufferMs_) + 100ms);
                                       }),
                        messages_.end());

        if (send_now)
            messages_.push_front(const_buf);
        else
            messages_.push_back(const_buf);

        if (messages_.size() > 1)
        {
            LOG(DEBUG, LOG_TAG) << "outstanding async_write\n";
            return;
        }
        send_next();
    });
}


void StreamSession::sendAsync(msg::message_ptr message, bool send_now)
{
    if (!message)
        return;

    // TODO: better set the timestamp in send_next for more accurate time sync
    sendAsync(shared_const_buffer(*message), send_now);
}


void StreamSession::setBufferMs(size_t bufferMs)
{
    bufferMs_ = bufferMs;
}
