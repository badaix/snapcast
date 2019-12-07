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

#include <fcntl.h>
#include <memory>
#include <sys/stat.h>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "encoder/encoder_factory.hpp"
#include "file_stream.hpp"


using namespace std;



FileStream::FileStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : PcmStream(pcmListener, ioc, uri)
{
    ifs.open(uri_.path.c_str(), std::ifstream::in | std::ifstream::binary);
    if (!ifs.good())
    {
        LOG(ERROR) << "failed to open PCM file: \"" + uri_.path + "\"\n";
        throw SnapException("failed to open PCM file: \"" + uri_.path + "\"");
    }
}


FileStream::~FileStream()
{
    ifs.close();
}


void FileStream::worker()
{
    timeval tvChunk;
    std::unique_ptr<msg::PcmChunk> chunk(new msg::PcmChunk(sampleFormat_, chunk_ms_));

    ifs.seekg(0, ifs.end);
    size_t length = ifs.tellg();
    ifs.seekg(0, ifs.beg);

    setState(ReaderState::kPlaying);

    while (active_)
    {
        chronos::systemtimeofday(&tvChunk);
        tvEncodedChunk_ = tvChunk;
        long nextTick = chronos::getTickCount();
        try
        {
            while (active_)
            {
                chunk->timestamp.sec = tvChunk.tv_sec;
                chunk->timestamp.usec = tvChunk.tv_usec;
                size_t toRead = chunk->payloadSize;
                size_t count = 0;

                size_t pos = ifs.tellg();
                size_t left = length - pos;
                if (left < toRead)
                {
                    ifs.read(chunk->payload, left);
                    ifs.seekg(0, ifs.beg);
                    count = left;
                }
                ifs.read(chunk->payload + count, toRead - count);

                encoder_->encode(chunk.get());
                if (!active_)
                    break;
                nextTick += chunk_ms_;
                chronos::addUs(tvChunk, chunk_ms_ * 1000);
                long currentTick = chronos::getTickCount();

                if (nextTick >= currentTick)
                {
                    //					LOG(INFO) << "sleep: " << nextTick - currentTick << "\n";
                    if (!sleep(nextTick - currentTick))
                        break;
                }
                else
                {
                    chronos::systemtimeofday(&tvChunk);
                    tvEncodedChunk_ = tvChunk;
                    pcmListener_->onResync(this, currentTick - nextTick);
                    nextTick = currentTick;
                }
            }
        }
        catch (const std::exception& e)
        {
            LOG(ERROR) << "(FileStream) Exception: " << e.what() << std::endl;
        }
    }
}
