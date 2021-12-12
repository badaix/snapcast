/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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
#include "posix_stream.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"

// standard headers
#include <cerrno>
#include <memory>


using namespace std;
using namespace std::chrono_literals;

namespace streamreader
{

static constexpr auto LOG_TAG = "PosixStream";
static constexpr auto kResyncTolerance = 50ms;

PosixStream::PosixStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : AsioStream<stream_descriptor>(pcmListener, ioc, server_settings, uri)
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

        if (first_)
        {
            LOG(TRACE, LOG_TAG) << "First read, initializing nextTick to now\n";
            nextTick_ = std::chrono::steady_clock::now();
        }

        int toRead = chunk_->payloadSize;
        auto duration = chunk_->duration<std::chrono::nanoseconds>();
        int len = 0;
        do
        {
            int count = read(stream_->native_handle(), chunk_->payload + len, toRead - len);
            if (count < 0)
            {
                // no data available, fill with silence
                memset(chunk_->payload + len, 0, toRead - len);

                // avoid overflow after 186min 24s silence (at 48000:16:2)
                if (idle_bytes_ <= max_idle_bytes_)
                    idle_bytes_ += toRead - len;
                break;
            }
            else if (count == 0)
            {
                throw SnapException("end of file");
            }
            else
            {
                // LOG(DEBUG, LOG_TAG) << "count: " << count << "\n";
                len += count;
                bytes_read_ += len;
                idle_bytes_ = 0;
            }
        } while (len < toRead);

        // LOG(DEBUG, LOG_TAG) << "Received " << len << "/" << toRead << " bytes\n";
        if (first_)
        {
            first_ = false;
            // initialize the stream's base timestamp to now minus the chunk's duration
            tvEncodedChunk_ = std::chrono::steady_clock::now() - duration;
        }

        if ((idle_bytes_ == 0) || (idle_bytes_ <= max_idle_bytes_))
        {
            // the encoder will update the tvEncodedChunk when a chunk is encoded
            chunkRead(*chunk_);
        }
        else
        {
            // no data available
            // set first_ = true will cause the timestamps to be updated without encoding
            first_ = true;
        }

        nextTick_ += duration;
        auto currentTick = std::chrono::steady_clock::now();
        auto next_read = nextTick_ - currentTick;
        if (next_read >= 0ms)
        {
            // synchronize reads to an interval of chunk_ms_
            wait(read_timer_, nextTick_ - currentTick, [this] { do_read(); });
            return;
        }
        else if (next_read >= -kResyncTolerance)
        {
            LOG(INFO, LOG_TAG) << "next read < 0 (" << getName() << "): " << std::chrono::duration_cast<std::chrono::microseconds>(next_read).count() / 1000.
                               << " ms\n";
            do_read();
        }
        else
        {
            // reading chunk_ms_ took longer than chunk_ms_
            resync(-next_read);
            first_ = true;
            wait(read_timer_, duration + kResyncTolerance, [this] { do_read(); });
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
