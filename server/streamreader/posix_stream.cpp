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

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "posix_stream.hpp"


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "PosixStream";


PosixStream::PosixStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : AsioStream<stream_descriptor>(pcmListener, ioc, uri)
{
    if (uri_.query.find("dryout_ms") != uri_.query.end())
        dryout_ms_ = cpt::stoul(uri_.query["dryout_ms"]);
    else
        dryout_ms_ = 2000;
}


void PosixStream::connect()
{
    if (!active_)
        return;

    try
    {
        do_connect();
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Connect exception: " << e.what() << "\n";
        auto self = this->shared_from_this();
        read_timer_.expires_after(std::chrono::milliseconds(100));
        read_timer_.async_wait([self, this](const boost::system::error_code& ec) {
            if (ec)
            {
                LOG(ERROR, LOG_TAG) << "Error during async wait: " << ec.message() << "\n";
            }
            else
            {
                connect();
            }
        });
    }
}


void PosixStream::do_disconnect()
{
    if (stream_->is_open())
        stream_->close();
}


void PosixStream::do_read()
{
    try
    {
        if (!stream_->is_open())
            throw SnapException("failed to open stream: \"" + uri_.path + "\"");

        int toRead = chunk_->payloadSize;
        int len = 0;
        do
        {
            int count = read(stream_->native_handle(), chunk_->payload + len, toRead - len);
            if (count < 0)
            {
                LOG(DEBUG, LOG_TAG) << "count < 0: " << errno
                                    << " && idleBytes < maxIdleBytes, ms: " << 1000 * chunk_->payloadSize / (sampleFormat_.rate * sampleFormat_.frameSize)
                                    << "\n";
                memset(chunk_->payload + len, 0, toRead - len);
                len += toRead - len;
                break;
            }
            else if (count == 0)
            {
                throw SnapException("end of file");
            }
            else
            {
                // LOG(DEBUG) << "count: " << count << "\n";
                len += count;
                bytes_read_ += len;
            }
        } while (len < toRead);

        if (first_)
        {
            first_ = false;
            chronos::systemtimeofday(&tvEncodedChunk_);
            nextTick_ = chronos::getTickCount() + buffer_ms_;
        }
        encoder_->encode(chunk_.get());
        nextTick_ += chunk_ms_;
        long currentTick = chronos::getTickCount();

        if (nextTick_ >= currentTick)
        {
            auto self = this->shared_from_this();
            read_timer_.expires_after(std::chrono::milliseconds(nextTick_ - currentTick));
            read_timer_.async_wait([self, this](const boost::system::error_code& ec) {
                if (ec)
                {
                    LOG(ERROR, LOG_TAG) << "Error during async wait: " << ec.message() << "\n";
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
            pcmListener_->onResync(this, currentTick - nextTick_);
            nextTick_ = currentTick + buffer_ms_;
            first_ = true;
            do_read();
        }

        lastException_ = "";
    }
    catch (const std::exception& e)
    {
        if (lastException_ != e.what())
        {
            LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << std::endl;
            lastException_ = e.what();
        }
        disconnect();
        connect();
    }
}

} // namespace streamreader
