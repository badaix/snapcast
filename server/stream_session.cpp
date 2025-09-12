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

// prototype/interface header file
#include "stream_session.hpp"

// local headers
#include "common/aixlog.hpp"

// 3rd party headers

// standard headers
#include <iostream>


using namespace std;
using namespace streamreader;

static constexpr auto LOG_TAG = "StreamSession";


StreamSession::StreamSession(const boost::asio::any_io_executor& executor, const ServerSettings& server_settings, StreamMessageReceiver* receiver)
    : authinfo(server_settings.auth), messageReceiver_(receiver), pcm_stream_(nullptr), strand_(boost::asio::make_strand(executor))
{
    base_msg_size_ = baseMessage_.getSize();
    buffer_.resize(base_msg_size_);
}


void StreamSession::setPcmStream(PcmStreamPtr pcmStream)
{
    std::lock_guard<std::mutex> lock(mutex_);
    pcm_stream_ = std::move(pcmStream);
}


const PcmStreamPtr StreamSession::pcmStream() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pcm_stream_;
}


void StreamSession::sendNext()
{
    auto& buffer = messages_.front();
    buffer.on_air = true;
    boost::asio::post(strand_, [this, self = shared_from_this(), buffer]()
    {
        sendAsync(buffer, [this, buffer](boost::system::error_code ec, std::size_t length)
        {
            auto write_handler = buffer.getWriteHandler();
            if (write_handler)
                write_handler(ec, length);

            messages_.pop_front();
            if (ec)
            {
                LOG(ERROR, LOG_TAG) << "StreamSession write error (msg length: " << length << "): " << ec.message() << "\n";
                messageReceiver_->onDisconnect(this);
                return;
            }
            if (!messages_.empty())
                sendNext();
        });
    });
}


void StreamSession::send(shared_const_buffer const_buf, WriteHandler&& handler)
{
    boost::asio::post(strand_, [this, self = shared_from_this(), const_buf = std::move(const_buf), handler = std::move(handler)]() mutable
    {
        // delete PCM chunks that are older than the overall buffer duration
        messages_.erase(std::remove_if(messages_.begin(), messages_.end(),
                                       [this](const shared_const_buffer& buffer)
        {
            const auto& msg = buffer.message();
            if (!msg.is_pcm_chunk || buffer.on_air)
                return false;
            auto age = chronos::clk::now() - msg.rec_time;
            return (age > std::chrono::milliseconds(bufferMs_) + 100ms);
        }),
                        messages_.end());

        const_buf.setWriteHandler(std::move(handler));
        messages_.push_back(std::move(const_buf));

        if (messages_.size() > 1)
        {
            LOG(TRACE, LOG_TAG) << "outstanding async_write\n";
            return;
        }
        sendNext();
    });
}


void StreamSession::send(const msg::message_ptr& message, WriteHandler&& handler)
{
    if (!message)
        return;

    // TODO: better set the timestamp in send_next for more accurate time sync
    send(shared_const_buffer(*message), std::move(handler));
}


void StreamSession::setBufferMs(size_t bufferMs)
{
    bufferMs_ = bufferMs;
}
