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

#ifndef ASIO_STREAM_HPP
#define ASIO_STREAM_HPP

#include "pcm_stream.hpp"
#include <atomic>
#include <boost/asio.hpp>



template <typename ReadStream>
class AsioStream : public PcmStream, public std::enable_shared_from_this<AsioStream<ReadStream>>
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    AsioStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri);

    void start() override;
    void stop() override;

protected:
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void on_connect();
    virtual void do_read();
    void check_state();
    std::unique_ptr<msg::PcmChunk> chunk_;
    timeval tv_chunk_;
    bool first_;
    long nextTick_;
    uint32_t buffer_ms_;
    boost::asio::deadline_timer timer_;
    boost::asio::deadline_timer idle_timer_;
    std::unique_ptr<ReadStream> stream_;
    std::atomic<std::uint64_t> bytes_read_;
};



template <typename ReadStream>
AsioStream<ReadStream>::AsioStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, uri), timer_(ioc), idle_timer_(ioc)
{
    chunk_ = std::make_unique<msg::PcmChunk>(sampleFormat_, pcmReadMs_);
    bytes_read_ = 0;

    buffer_ms_ = 50;
    try
    {
        buffer_ms_ = cpt::stoi(uri_.getQuery("bufferms", cpt::to_string(buffer_ms_)));
    }
    catch (...)
    {
    }
}


template <typename ReadStream>
void AsioStream<ReadStream>::check_state()
{
    uint64_t last_read = bytes_read_;
    auto self = this->shared_from_this();
    idle_timer_.expires_from_now(boost::posix_time::milliseconds(500 + pcmReadMs_));
    idle_timer_.async_wait([self, this, last_read](const boost::system::error_code& ec) {
        if (!ec)
        {
            LOG(DEBUG) << "check state last: " << last_read << ", read: " << bytes_read_ << "\n";
            if (bytes_read_ != last_read)
                setState(kPlaying);
            else
                setState(kIdle);
            check_state();
        }
    });
}


template <typename ReadStream>
void AsioStream<ReadStream>::start()
{
    encoder_->init(this, sampleFormat_);
    check_state();
    connect();
}


template <typename ReadStream>
void AsioStream<ReadStream>::stop()
{
    timer_.cancel();
    idle_timer_.cancel();
    disconnect();
}


template <typename ReadStream>
void AsioStream<ReadStream>::on_connect()
{
    chronos::systemtimeofday(&tv_chunk_);
    tvEncodedChunk_ = tv_chunk_;
    nextTick_ = chronos::getTickCount();
    first_ = true;
    do_read();
}


template <typename ReadStream>
void AsioStream<ReadStream>::do_read()
{
    // LOG(DEBUG) << "do_read\n";
    auto self = this->shared_from_this();
    chunk_->timestamp.sec = tv_chunk_.tv_sec;
    chunk_->timestamp.usec = tv_chunk_.tv_usec;
    boost::asio::async_read(*stream_, boost::asio::buffer(chunk_->payload, chunk_->payloadSize),
                            [this, self](boost::system::error_code ec, std::size_t length) mutable {
                                if (ec)
                                {
                                    LOG(ERROR) << "Error reading message: " << ec.message() << ", length: " << length << "\n";
                                    connect();
                                    return;
                                }

                                bytes_read_ += length;
                                // LOG(DEBUG) << "Read: " << length << " bytes\n";
                                if (first_)
                                {
                                    first_ = false;
                                    chronos::systemtimeofday(&tv_chunk_);
                                    chunk_->timestamp.sec = tv_chunk_.tv_sec;
                                    chunk_->timestamp.usec = tv_chunk_.tv_usec;
                                    tvEncodedChunk_ = tv_chunk_;
                                    nextTick_ = chronos::getTickCount() + buffer_ms_;
                                }
                                encoder_->encode(chunk_.get());
                                nextTick_ += pcmReadMs_;
                                chronos::addUs(tv_chunk_, pcmReadMs_ * 1000);
                                long currentTick = chronos::getTickCount();

                                if (nextTick_ >= currentTick)
                                {
                                    timer_.expires_from_now(boost::posix_time::milliseconds(nextTick_ - currentTick));
                                    timer_.async_wait([self, this](const boost::system::error_code& ec) {
                                        if (ec)
                                        {
                                            LOG(ERROR) << "Error during async wait: " << ec.message() << "\n";
                                        }
                                        else
                                        {
                                            do_read();
                                        }
                                    });
                                    return;
                                }
                                else
                                {
                                    chronos::systemtimeofday(&tv_chunk_);
                                    tvEncodedChunk_ = tv_chunk_;
                                    pcmListener_->onResync(this, currentTick - nextTick_);
                                    nextTick_ = currentTick + buffer_ms_;
                                    do_read();
                                }
                            });
}



#endif
