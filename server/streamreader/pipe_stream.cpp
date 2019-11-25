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
#include "encoder/encoder_factory.hpp"
#include "pipe_stream.hpp"


using namespace std;



PipeStream::PipeStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : PcmStream(pcmListener, ioc, uri), timer_(ioc)
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


PipeStream::~PipeStream()
{
    fifo_->close();
}


void PipeStream::start()
{
    encoder_->init(this, sampleFormat_);
    active_ = true;
    do_accept();
}


void PipeStream::do_accept()
{
    LOG(DEBUG) << "do_accept\n";
    auto self = shared_from_this();
    auto fd = open(uri_.path.c_str(), O_RDONLY | O_NONBLOCK);
    fifo_ = std::make_unique<boost::asio::posix::stream_descriptor>(ioc_, fd);
    chronos::systemtimeofday(&tv_chunk_);
    tvEncodedChunk_ = tv_chunk_;
    nextTick_ = chronos::getTickCount();
    first_ = true;
    do_read();
}


void PipeStream::do_read()
{
    // LOG(DEBUG) << "do_read\n";
    auto self = shared_from_this();
    chunk_->timestamp.sec = tv_chunk_.tv_sec;
    chunk_->timestamp.usec = tv_chunk_.tv_usec;
    boost::asio::async_read(*fifo_, boost::asio::buffer(chunk_->payload, chunk_->payloadSize),
                            [this, self](boost::system::error_code ec, std::size_t length) mutable {
                                if (ec)
                                {
                                    LOG(ERROR) << "Error reading message: " << ec.message() << ", length: " << length << "\n";
                                    do_accept();
                                    return;
                                }
                                // LOG(DEBUG) << "Read: " << length << " bytes\n";
                                if (first_)
                                {
                                    first_ = false;
                                    chronos::systemtimeofday(&tv_chunk_);
                                    chunk_->timestamp.sec = tv_chunk_.tv_sec;
                                    chunk_->timestamp.usec = tv_chunk_.tv_usec;
                                    tvEncodedChunk_ = tv_chunk_;
                                    nextTick_ = chronos::getTickCount();
                                }
                                encoder_->encode(chunk_.get());
                                nextTick_ += pcmReadMs_;
                                chronos::addUs(tv_chunk_, pcmReadMs_ * 1000);
                                long currentTick = chronos::getTickCount();

                                if (nextTick_ >= currentTick)
                                {
                                    setState(kPlaying);
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
                                    nextTick_ = currentTick;
                                    do_read();
                                }
                            });
}



// void PipeStream::worker()
// {
//     timeval tvChunk;
//     std::unique_ptr<msg::PcmChunk> chunk(new msg::PcmChunk(sampleFormat_, pcmReadMs_));
//     string lastException = "";

//     while (active_)
//     {
//         if (fd_ != -1)
//             close(fd_);
//         fd_ = open(uri_.path.c_str(), O_RDONLY | O_NONBLOCK);
//         chronos::systemtimeofday(&tvChunk);
//         tvEncodedChunk_ = tvChunk;
//         long nextTick = chronos::getTickCount();
//         int idleBytes = 0;
//         int maxIdleBytes = sampleFormat_.rate * sampleFormat_.frameSize * dryoutMs_ / 1000;
//         try
//         {
//             if (fd_ == -1)
//                 throw SnapException("failed to open fifo: \"" + uri_.path + "\"");

//             while (active_)
//             {
//                 chunk->timestamp.sec = tvChunk.tv_sec;
//                 chunk->timestamp.usec = tvChunk.tv_usec;
//                 int toRead = chunk->payloadSize;
//                 int len = 0;
//                 do
//                 {
//                     int count = read(fd_, chunk->payload + len, toRead - len);
//                     if (count < 0 && idleBytes < maxIdleBytes)
//                     {
//                         memset(chunk->payload + len, 0, toRead - len);
//                         idleBytes += toRead - len;
//                         len += toRead - len;
//                         continue;
//                     }
//                     if (count < 0)
//                     {
//                         setState(kIdle);
//                         if (!sleep(100))
//                             break;
//                     }
//                     else if (count == 0)
//                         throw SnapException("end of file");
//                     else
//                     {
//                         len += count;
//                         idleBytes = 0;
//                     }
//                 } while ((len < toRead) && active_);

//                 if (!active_)
//                     break;

//                 /// TODO: use less raw pointers, make this encoding more transparent
//                 encoder_->encode(chunk.get());

//                 if (!active_)
//                     break;

//                 nextTick += pcmReadMs_;
//                 chronos::addUs(tvChunk, pcmReadMs_ * 1000);
//                 long currentTick = chronos::getTickCount();

//                 if (nextTick >= currentTick)
//                 {
//                     setState(kPlaying);
//                     if (!sleep(nextTick - currentTick))
//                         break;
//                 }
//                 else
//                 {
//                     chronos::systemtimeofday(&tvChunk);
//                     tvEncodedChunk_ = tvChunk;
//                     pcmListener_->onResync(this, currentTick - nextTick);
//                     nextTick = currentTick;
//                 }

//                 lastException = "";
//             }
//         }
//         catch (const std::exception& e)
//         {
//             if (lastException != e.what())
//             {
//                 LOG(ERROR) << "(PipeStream) Exception: " << e.what() << std::endl;
//                 lastException = e.what();
//             }
//             if (!sleep(100))
//                 break;
//         }
//     }
// }
