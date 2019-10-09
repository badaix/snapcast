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

using namespace std;



StreamSession::StreamSession(MessageReceiver* receiver, tcp::socket&& socket)
    : active_(false), writerThread_(nullptr), socket_(std::move(socket)), messageReceiver_(receiver), pcmStream_(nullptr)
{
    base_msg_size_ = baseMessage_.getSize();
    buffer_.resize(base_msg_size_);
}


StreamSession::~StreamSession()
{
    stop();
}


void StreamSession::setPcmStream(PcmStreamPtr pcmStream)
{
    pcmStream_ = pcmStream;
}


const PcmStreamPtr StreamSession::pcmStream() const
{
    return pcmStream_;
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


void StreamSession::start()
{
    active_ = true;
    //   readerThread_.reset(new thread(&StreamSession::reader, this));
    read_header();
    writerThread_.reset(new thread(&StreamSession::writer, this));
}


void StreamSession::stop()
{
    if (!active_)
        return;

    active_ = false;

    try
    {
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
            LOG(ERROR) << "Error in socket shutdown: " << ec.message() << "\n";
        socket_.close(ec);
        if (ec)
            LOG(ERROR) << "Error in socket close: " << ec.message() << "\n";
        if (writerThread_ && writerThread_->joinable())
        {
            LOG(DEBUG) << "StreamSession joining writerThread\n";
            messages_.abort_wait();
            writerThread_->join();
        }
    }
    catch (...)
    {
    }

    writerThread_ = nullptr;
    LOG(DEBUG) << "StreamSession stopped\n";
}


void StreamSession::sendAsync(const msg::message_ptr& message, bool sendNow)
{
    if (!message)
        return;

    // the writer will take care about old messages
    while (messages_.size() > 2000) // chunk->getDuration() > 10000)
        messages_.pop();

    if (sendNow)
        messages_.push_front(message);
    else
        messages_.push(message);
}


bool StreamSession::active() const
{
    return active_;
}


void StreamSession::setBufferMs(size_t bufferMs)
{
    bufferMs_ = bufferMs;
}


bool StreamSession::send(const msg::message_ptr& message)
{
    // TODO on exception: set active = false
    //	LOG(INFO) << "send: " << message->type << ", size: " << message->getSize() << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
    if (!active_)
        return false;

    boost::asio::streambuf streambuf;
    std::ostream stream(&streambuf);
    tv t;
    message->sent = t;
    message->serialize(stream);
    boost::asio::write(socket_, streambuf);
    //	LOG(INFO) << "done: " << message->type << ", size: " << message->size << ", id: " << message->id << ", refers: " << message->refersTo << "\n";
    return true;
}



void StreamSession::writer()
{
    try
    {
        boost::asio::streambuf streambuf;
        std::ostream stream(&streambuf);
        shared_ptr<msg::BaseMessage> message;
        while (active_)
        {
            if (messages_.try_pop(message, std::chrono::milliseconds(500)))
            {
                if (bufferMs_ > 0)
                {
                    const msg::WireChunk* wireChunk = dynamic_cast<const msg::WireChunk*>(message.get());
                    if (wireChunk != nullptr)
                    {
                        chronos::time_point_clk now = chronos::clk::now();
                        size_t age = 0;
                        if (now > wireChunk->start())
                            age = std::chrono::duration_cast<chronos::msec>(now - wireChunk->start()).count();
                        // LOG(DEBUG) << "PCM chunk. Age: " << age << ", buffer: " << bufferMs_ << ", age > buffer: " << (age > bufferMs_) << "\n";
                        if (age > bufferMs_)
                            continue;
                    }
                }
                send(message);
            }
        }
    }
    catch (const std::exception& e)
    {
        SLOG(ERROR) << "Exception in StreamSession::writer(): " << e.what() << endl;
    }

    if (active_ && (messageReceiver_ != nullptr))
        messageReceiver_->onDisconnect(this);
}
