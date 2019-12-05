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
#include "pipe_stream.hpp"


using namespace std;



PipeStream::PipeStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : AsioStream<stream_descriptor>(pcmListener, ioc, uri)
{
    umask(0);
    string mode = uri_.getQuery("mode", "create");

    LOG(INFO) << "PipeStream mode: " << mode << "\n";
    if ((mode != "read") && (mode != "create"))
        throw SnapException("create mode for fifo must be \"read\" or \"create\"");

    if (mode == "create")
    {
        if ((mkfifo(uri_.path.c_str(), 0666) != 0) && (errno != EEXIST))
            throw SnapException("failed to make fifo \"" + uri_.path + "\": " + cpt::to_string(errno));
    }
    chunk_ = make_unique<msg::PcmChunk>(sampleFormat_, pcmReadMs_);
}


void PipeStream::connect()
{
    LOG(DEBUG) << "connect\n";
    auto self = shared_from_this();
    fd_ = open(uri_.path.c_str(), O_RDONLY | O_NONBLOCK);
    stream_ = std::make_unique<boost::asio::posix::stream_descriptor>(ioc_, fd_);
    on_connect();
}


void PipeStream::disconnect()
{
    stream_->close();
}


void PipeStream::do_read()
{
    auto self = this->shared_from_this();
    try
    {
        if (fd_ == -1)
            throw SnapException("failed to open fifo: \"" + uri_.path + "\"");

        int toRead = chunk_->payloadSize;
        int len = 0;
        do
        {
            int count = read(fd_, chunk_->payload + len, toRead - len);
            if (count < 0)
            {
                LOG(DEBUG) << "count < 0: " << errno << " && idleBytes < maxIdleBytes, ms: " << 1000 * chunk_->payloadSize / (sampleFormat_.rate * sampleFormat_.frameSize) << "\n";
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
        nextTick_ += pcmReadMs_;
        long currentTick = chronos::getTickCount();

        if (nextTick_ >= currentTick)
        {
            read_timer_.expires_after(std::chrono::milliseconds(nextTick_ - currentTick));
            read_timer_.async_wait([self, this](const boost::system::error_code& ec) {
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
            LOG(ERROR) << "(PipeStream) Exception: " << e.what() << std::endl;
            lastException_ = e.what();
        }
        connect();
    }
}
