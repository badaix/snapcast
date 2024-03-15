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

#ifndef ASIO_STREAM_HPP
#define ASIO_STREAM_HPP

// local headers
#include "common/aixlog.hpp"
#include "common/str_compat.hpp"
#include "pcm_stream.hpp"

// 3rd party headers
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <atomic>


namespace streamreader
{

using namespace std::chrono_literals;

template <typename ReadStream>
class AsioStream : public PcmStream
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    AsioStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);

    void start() override;
    void stop() override;

protected:
    virtual void connect() = 0;
    virtual void disconnect();

    virtual void on_connect();
    virtual void do_read();
    /// Start a timer that will change the stream state to idle after \p duration
    void check_state(const std::chrono::steady_clock::duration& duration);

    /// Use Timer \p timer to call \p handler after \p duration
    template <typename Timer, typename Rep, typename Period>
    void wait(Timer& timer, const std::chrono::duration<Rep, Period>& duration, std::function<void()> handler);

    /// Cache last exception to avoid repeated error logging
    std::string lastException_;
    timeval tv_chunk_;
    bool first_;
    std::chrono::time_point<std::chrono::steady_clock> nextTick_;
    uint32_t buffer_ms_;
    boost::asio::steady_timer read_timer_;
    boost::asio::steady_timer state_timer_;
    std::unique_ptr<ReadStream> stream_;

    /// duration of the current silence period
    std::chrono::microseconds silence_{0ms};
    /// silence duration before switching the stream to idle
    std::chrono::milliseconds idle_threshold_;
};


template <typename ReadStream>
template <typename Timer, typename Rep, typename Period>
void AsioStream<ReadStream>::wait(Timer& timer, const std::chrono::duration<Rep, Period>& duration, std::function<void()> handler)
{
    timer.expires_after(duration);
    timer.async_wait(
        [handler = std::move(handler)](const boost::system::error_code& ec)
        {
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
AsioStream<ReadStream>::AsioStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, server_settings, uri), read_timer_(strand_), state_timer_(strand_)
{
    LOG(DEBUG, "AsioStream") << "Chunk duration: " << chunk_->durationMs() << " ms, frames: " << chunk_->getFrameCount() << ", size: " << chunk_->payloadSize
                             << "\n";

    idle_threshold_ = std::chrono::milliseconds(std::max(cpt::stoi(uri_.getQuery("idle_threshold", "100")), 10));

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
void AsioStream<ReadStream>::check_state(const std::chrono::steady_clock::duration& duration)
{
    state_timer_.expires_after(duration);
    state_timer_.async_wait(
        [this, duration](const boost::system::error_code& ec)
        {
        if (!ec)
        {
            LOG(INFO, "AsioStream") << "No data since " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                                    << " ms, switching to idle\n";
            setState(ReaderState::kIdle);
        }
    });
}


template <typename ReadStream>
void AsioStream<ReadStream>::start()
{
    PcmStream::start();
    connect();
}


template <typename ReadStream>
void AsioStream<ReadStream>::stop()
{
    read_timer_.cancel();
    disconnect();
    PcmStream::stop();
}


template <typename ReadStream>
void AsioStream<ReadStream>::disconnect()
{
    if (stream_ && stream_->is_open())
        stream_->close();
    setState(ReaderState::kIdle);
}


template <typename ReadStream>
void AsioStream<ReadStream>::on_connect()
{
    first_ = true;
    tvEncodedChunk_ = std::chrono::steady_clock::now();
    do_read();
}


template <typename ReadStream>
void AsioStream<ReadStream>::do_read()
{
    // Reset the silence timer
    check_state(idle_threshold_ + std::chrono::milliseconds(chunk_ms_));
    boost::asio::async_read(*stream_, boost::asio::buffer(chunk_->payload, chunk_->payloadSize),
                            [this](boost::system::error_code ec, std::size_t length) mutable
                            {
        state_timer_.cancel();

        if (ec)
        {
            if (lastException_ != ec.message())
            {
                LOG(ERROR, "AsioStream") << "Error reading message: " << ec.message() << ", length: " << length << ", ec: " << ec << "\n";
                lastException_ = ec.message();
            }
            disconnect();
            wait(read_timer_, 100ms, [this] { connect(); });
            return;
        }

        lastException_.clear();

        if (isSilent(*chunk_))
        {
            silence_ += chunk_->duration<std::chrono::microseconds>();
            if (silence_ >= idle_threshold_)
            {
                setState(ReaderState::kIdle);
                // Avoid overflow
                silence_ = idle_threshold_;
            }
        }
        else
        {
            silence_ = 0ms;
            setState(ReaderState::kPlaying);
        }

        // LOG(DEBUG, "AsioStream") << "Read: " << length << " bytes\n";
        // First read after connect. Set the initial read timestamp
        // the timestamp will be incremented after encoding,
        // since we do not know how much the encoder actually encoded

        // if (!first_)
        // {
        //     auto now = std::chrono::steady_clock::now();
        //     auto stream2systime_diff = now - tvEncodedChunk_;
        //     if (stream2systime_diff > chronos::sec(5) + chronos::msec(chunk_ms_))
        //     {
        //         LOG(WARNING, "AsioStream") << "Stream and system time out of sync: "
        //                                    << std::chrono::duration_cast<std::chrono::microseconds>(stream2systime_diff).count() / 1000.
        //                                    << " ms, resetting stream time.\n";
        //         first_ = true;
        //     }
        // }
        if (first_)
        {
            first_ = false;
            tvEncodedChunk_ = std::chrono::steady_clock::now() - chunk_->duration<std::chrono::nanoseconds>();
            nextTick_ = std::chrono::steady_clock::now();
        }

        chunkRead(*chunk_);
        nextTick_ += chunk_->duration<std::chrono::nanoseconds>();
        auto currentTick = std::chrono::steady_clock::now();

        // Synchronize read to chunk_ms_
        if (nextTick_ >= currentTick)
        {
            read_timer_.expires_after(nextTick_ - currentTick);
            read_timer_.async_wait(
                [this](const boost::system::error_code& ec)
                {
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
            resync(std::chrono::duration_cast<std::chrono::nanoseconds>(currentTick - nextTick_));
            first_ = true;
            do_read();
        }
    });
}

} // namespace streamreader

#endif
