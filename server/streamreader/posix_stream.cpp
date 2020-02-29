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
using namespace std::chrono_literals;

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

    idle_bytes_ = 0;
    max_idle_bytes_ = sampleFormat_.rate() * sampleFormat_.frameSize() * dryout_ms_ / 1000;

    try
    {
        do_connect();
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Connect exception: " << e.what() << "\n";
        wait(read_timer_, 100ms, [this] { connect(); });
    }
}


void PosixStream::do_disconnect()
{
    if (stream_ && stream_->is_open())
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
            if (count < 0 && idle_bytes_ < max_idle_bytes_)
            {
                // nothing to read for a longer time now, set the chunk to silent
                LOG(DEBUG, LOG_TAG) << "count < 0: " << errno
                                    << " && idleBytes < maxIdleBytes, ms: " << 1000 * chunk_->payloadSize / (sampleFormat_.rate() * sampleFormat_.frameSize())
                                    << "\n";
                memset(chunk_->payload + len, 0, toRead - len);
                idle_bytes_ += toRead - len;
                len += toRead - len;
                break;
            }
            else if (count < 0)
            {
                // nothing to read, try again (chunk_ms_ / 2) later
                wait(read_timer_, std::chrono::milliseconds(chunk_ms_ / 2), [this] { do_read(); });
                return;
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
                idle_bytes_ = 0;
            }
        } while (len < toRead);

        if (first_)
        {
            first_ = false;
            chronos::systemtimeofday(&tvEncodedChunk_);
            nextTick_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(buffer_ms_);
        }
        encoder_->encode(chunk_.get());
        nextTick_ += chunk_->duration<std::chrono::nanoseconds>();
        auto currentTick = std::chrono::steady_clock::now();

        if (nextTick_ >= currentTick)
        {
            // synchronize reads to an interval of chunk_ms_
            wait(read_timer_, nextTick_ - currentTick, [this] { do_read(); });
            return;
        }
        else
        {
            // reading chunk_ms_ took longer than chunk_ms_
            pcmListener_->onResync(this, std::chrono::duration_cast<std::chrono::milliseconds>(currentTick - nextTick_).count());
            nextTick_ = currentTick + std::chrono::milliseconds(buffer_ms_);
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
        wait(read_timer_, 100ms, [this] { connect(); });
    }
}

} // namespace streamreader
