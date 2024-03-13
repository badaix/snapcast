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

// prototype/interface header file
#include "pipe_stream.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"

// standard headers
#include <cerrno>
#include <memory>


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "PipeStream";


PipeStream::PipeStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : AsioStream<stream_descriptor>(pcmListener, ioc, server_settings, uri)
{
    umask(0);
    string mode = uri_.getQuery("mode", "create");

    LOG(INFO, LOG_TAG) << "PipeStream mode: " << mode << "\n";
    if ((mode != "read") && (mode != "create"))
        throw SnapException(R"(create mode for fifo must be "read" or "create")");

    if (mode == "create")
    {
        if ((mkfifo(uri_.path.c_str(), 0666) != 0) && (errno != EEXIST))
            throw SnapException("failed to make fifo \"" + uri_.path + "\": " + cpt::to_string(errno));
    }
}


void PipeStream::connect()
{
    int fd = open(uri_.path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
        throw SnapException("failed to open fifo \"" + uri_.path + "\": " + cpt::to_string(errno));

    int pipe_size = -1;
#if !defined(MACOS) && !defined(FREEBSD)
    pipe_size = fcntl(fd, F_GETPIPE_SZ);
#endif
    LOG(TRACE, LOG_TAG) << "Stream: " << name_ << ", connect to pipe: " << uri_.path << ", fd: " << fd << ", pipe size: " << pipe_size << "\n";
    stream_ = std::make_unique<boost::asio::posix::stream_descriptor>(strand_, fd);
    on_connect();
}

} // namespace streamreader
