/***
    This file is part of snapcast
    Copyright (C) 2014-2019  Johannes Pohl

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
#include "message/pcmChunk.h"
#include <iostream>
#include <mutex>

using namespace std;



StreamSession::StreamSession(MessageReceiver* receiver, tcp::socket&& socket)
    : buffer_pos_(0), socket_(std::move(socket)), messageReceiver_(receiver), pcmStream_(nullptr)
{
    base_msg_size_ = baseMessage_.getSize();
    buffer_.resize(base_msg_size_);
}


StreamSession::~StreamSession()
{
    stop();
}


void StreamSession::read_message()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(&buffer_[buffer_pos_], baseMessage_.size - buffer_pos_),
                            [this, self](boost::system::error_code ec, std::size_t length) {
                                if (!ec)
                                {
                                    buffer_pos_ += length;
                                    if (buffer_pos_ + 1 < baseMessage_.size)
                                        read_message();
                                    else
                                    {
                                        tv t;
                                        baseMessage_.received = t;
                                        if (messageReceiver_ != nullptr)
                                            messageReceiver_->onMessageReceived(this, baseMessage_, &buffer_[0]);
                                        buffer_pos_ = 0;
                                        read_header();
                                    }
                                }
                            });
}

void StreamSession::read_header()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(&buffer_[buffer_pos_], base_msg_size_ - buffer_pos_),
                            [this, self](boost::system::error_code ec, std::size_t length) {
                                if (!ec)
                                {
                                    buffer_pos_ += length;
                                    if (buffer_pos_ + 1 < base_msg_size_)
                                        read_header();
                                    else
                                    {
                                        baseMessage_.deserialize(&buffer_[0]);

                                        if ((baseMessage_.type > message_type::kLast) || (baseMessage_.type < message_type::kFirst))
                                        {
                                            stringstream ss;
                                            ss << "unknown message type received: " << baseMessage_.type << ", size: " << baseMessage_.size;
                                            throw std::runtime_error(ss.str().c_str());
                                        }
                                        else if (baseMessage_.size > msg::max_size)
                                        {
                                            stringstream ss;
                                            ss << "received message of type " << baseMessage_.type << " to large: " << baseMessage_.size;
                                            throw std::runtime_error(ss.str().c_str());
                                        }

                                        //	LOG(INFO) << "getNextMessage: " << baseMessage.type << ", size: " << baseMessage.size << ", id: " <<
                                        // baseMessage.id << ", refers: " <<
                                        // baseMessage.refersTo << "\n";
                                        if (baseMessage_.size > buffer_.size())
                                            buffer_.resize(baseMessage_.size);
                                        buffer_pos_ = 0;
                                        read_message();
                                    }
                                }
                            });
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
    read_header();
}


void StreamSession::stop()
{
    LOG(DEBUG) << "StreamSession::stop\n";
    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
        LOG(ERROR) << "Error in socket shutdown: " << ec.message() << "\n";
    socket_.close(ec);
    if (ec)
        LOG(ERROR) << "Error in socket close: " << ec.message() << "\n";
    LOG(DEBUG) << "StreamSession stopped\n";
}


void StreamSession::sendAsync(msg::message_ptr message, bool sendNow)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!message)
        return;

    boost::asio::streambuf streambuf;
    std::ostream stream(&streambuf);
    tv t;
    message->sent = t;
    message->serialize(stream);

    auto self(shared_from_this());
    boost::asio::async_write(socket_, streambuf, [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec)
        {
            LOG(DEBUG) << "StreamSession wrote " << length << " bytes\n";
            // do_read();
        }
        else
        {
            LOG(ERROR) << "StreamSession write error: " << ec.message() << "\n";
        }
    });
}


void StreamSession::setBufferMs(size_t bufferMs)
{
    bufferMs_ = bufferMs;
}


bool StreamSession::send(msg::message_ptr message)
{
    sendAsync(message);
    // // TODO on exception: set active = false
    // //	LOG(INFO) << "send: " << message->type << ", size: " << message->getSize() << ", id: " << message->id << ", refers: " << message->refersTo <<
    // "\n"; boost::asio::streambuf streambuf; std::ostream stream(&streambuf); tv t; message->sent = t; message->serialize(stream); boost::asio::write(socket_,
    // streambuf);
    // //	LOG(INFO) << "done: " << message->type << ", size: " << message->size << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
    // return true;
}



// void StreamSession::reader()
// {
//     try
//     {
//         while (active_)
//         {
//             getNextMessage();
//         }
//     }
//     catch (const std::exception& e)
//     {
//         SLOG(ERROR) << "Exception in StreamSession::reader(): " << e.what() << endl;
//     }

//     if (active_ && (messageReceiver_ != nullptr))
//         messageReceiver_->onDisconnect(this);
// }


// void StreamSession::writer()
// {
//     try
//     {
//         boost::asio::streambuf streambuf;
//         std::ostream stream(&streambuf);
//         shared_ptr<msg::BaseMessage> message;
//         while (active_)
//         {
//             if (messages_.try_pop(message, std::chrono::milliseconds(500)))
//             {
//                 if (bufferMs_ > 0)
//                 {
//                     const msg::WireChunk* wireChunk = dynamic_cast<const msg::WireChunk*>(message.get());
//                     if (wireChunk != nullptr)
//                     {
//                         chronos::time_point_clk now = chronos::clk::now();
//                         size_t age = 0;
//                         if (now > wireChunk->start())
//                             age = std::chrono::duration_cast<chronos::msec>(now - wireChunk->start()).count();
//                         // LOG(DEBUG) << "PCM chunk. Age: " << age << ", buffer: " << bufferMs_ << ", age > buffer: " << (age > bufferMs_) << "\n";
//                         if (age > bufferMs_)
//                             continue;
//                     }
//                 }
//                 send(message);
//             }
//         }
//     }
//     catch (const std::exception& e)
//     {
//         SLOG(ERROR) << "Exception in StreamSession::writer(): " << e.what() << endl;
//     }

//     if (active_ && (messageReceiver_ != nullptr))
//         messageReceiver_->onDisconnect(this);
// }
