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

#ifndef ASIO_STREAM_HPP
#define ASIO_STREAM_HPP

#include "common/aixlog.hpp"
#include "common/str_compat.hpp"
#include "pcm_stream.hpp"
#include <atomic>
#include <boost/asio.hpp>

namespace streamreader
{

template <typename ReadStream>
class AsioStream : public PcmStream
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    AsioStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri);

    void start() override;
    void stop() override;

    virtual void connect();
    virtual void disconnect();

protected:
    virtual void do_connect() = 0;
    virtual void do_disconnect() = 0;
    virtual void on_connect();
    virtual void do_read();
    void check_state();

    template <typename Timer, typename Rep, typename Period>
    void wait(Timer& timer, const std::chrono::duration<Rep, Period>& duration, std::function<void()> handler);

    std::unique_ptr<msg::PcmChunk> chunk_;
    timeval tv_chunk_;
    bool first_;
    long nextTick_;
    uint32_t buffer_ms_;
    boost::asio::steady_timer read_timer_;
    boost::asio::steady_timer state_timer_;
    std::unique_ptr<ReadStream> stream_;
    std::atomic<std::uint64_t> bytes_read_;
};


template <typename ReadStream>
template <typename Timer, typename Rep, typename Period>
void AsioStream<ReadStream>::wait(Timer& timer, const std::chrono::duration<Rep, Period>& duration, std::function<void()> handler)
{
    timer.expires_after(duration);
    timer.async_wait([handler = std::move(handler)](const boost::system::error_code& ec) {
        if (ec)
        {
            LOG(ERROR, "AsioStream") << "Error during async wait: " << ec.message() << "\n";
        }
        else
        {
            handler();
        }
    });
}


template <typename ReadStream>
AsioStream<ReadStream>::AsioStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, uri), read_timer_(ioc), state_timer_(ioc)
{
    chunk_ = std::make_unique<msg::PcmChunk>(sampleFormat_, chunk_ms_);
    bytes_read_ = 0;
    buffer_ms_ = 50;

    try
    {
        buffer_ms_ = cpt::stoi(uri_.getQuery("buffer_ms", cpt::to_string(buffer_ms_)));
    }
    catch (...)
    {
    }
}


template <typename ReadStream>
void AsioStream<ReadStream>::check_state()
{
    uint64_t last_read = bytes_read_;
    wait(state_timer_, std::chrono::milliseconds(500 + chunk_ms_), [this, last_read] {
        LOG(TRACE, "AsioStream") << "check state last: " << last_read << ", read: " << bytes_read_ << "\n";
        if (bytes_read_ != last_read)
            setState(ReaderState::kPlaying);
        else
            setState(ReaderState::kIdle);
        check_state();
    });
}


template <typename ReadStream>
void AsioStream<ReadStream>::start()
{
    encoder_->init(this, sampleFormat_);
    active_ = true;
    check_state();
    connect();
}


template <typename ReadStream>
void AsioStream<ReadStream>::connect()
{
    do_connect();
}


template <typename ReadStream>
void AsioStream<ReadStream>::disconnect()
{
    do_disconnect();
}


template <typename ReadStream>
void AsioStream<ReadStream>::stop()
{
    active_ = false;
    read_timer_.cancel();
    state_timer_.cancel();
    disconnect();
}


template <typename ReadStream>
void AsioStream<ReadStream>::on_connect()
{
    first_ = true;
    chronos::systemtimeofday(&tvEncodedChunk_);
    do_read();
}


template <typename ReadStream>
void AsioStream<ReadStream>::do_read()
{
    // LOG(DEBUG, "AsioStream") << "do_read\n";
    boost::asio::async_read(
        *stream_, boost::asio::buffer(chunk_->payload, chunk_->payloadSize), [this](boost::system::error_code ec, std::size_t length) mutable {
            if (ec)
            {
                LOG(ERROR, "AsioStream") << "Error reading message: " << ec.message() << ", length: " << length << "\n";
                connect();
                return;
            }

            bytes_read_ += length;
            // LOG(DEBUG, "AsioStream") << "Read: " << length << " bytes\n";
            // First read after connect. Set the initial read timestamp
            // the timestamp will be incremented after encoding,
            // since we do not know how much the encoder actually encoded

            if (!first_)
            {
                timeval now;
                chronos::systemtimeofday(&now);
                auto stream2systime_diff = chronos::diff<std::chrono::milliseconds>(now, tvEncodedChunk_);
                if (stream2systime_diff > chronos::sec(5) + chronos::msec(chunk_ms_))
                {
                    LOG(WARNING, "AsioStream") << "Stream and system time out of sync: " << stream2systime_diff.count() << "ms, resetting stream time.\n";
                    first_ = true;
                }
            }
            if (first_)
            {
                first_ = false;
                chronos::systemtimeofday(&tvEncodedChunk_);
                nextTick_ = chronos::getTickCount() + buffer_ms_;
            }

            encoder_->encode(chunk_.get());
            nextTick_ += chunk_ms_;
            long currentTick = chronos::getTickCount();

            // Synchronize read to chunk_ms_
            if (nextTick_ >= currentTick)
            {
                read_timer_.expires_after(std::chrono::milliseconds(nextTick_ - currentTick));
                read_timer_.async_wait([this](const boost::system::error_code& ec) {
                    if (ec)
                    {
                        LOG(ERROR, "AsioStream") << "Error during async wait: " << ec.message() << "\n";
                    }
                    else
                    {
                        do_read();
                    }
                });
                return;
            }
            // Read took longer, wait for the buffer to fill up
            else
            {
                pcmListener_->onResync(this, currentTick - nextTick_);
                nextTick_ = currentTick + buffer_ms_;
                first_ = true;
                do_read();
            }
        });
}

} // namespace streamreader

#endif
