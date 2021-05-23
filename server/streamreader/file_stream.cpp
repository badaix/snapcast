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

#include <fcntl.h>
#include <memory>
#include <sys/stat.h>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "encoder/encoder_factory.hpp"
#include "file_stream.hpp"


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "FileStream";


FileStream::FileStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : PosixStream(pcmListener, ioc, server_settings, uri)
{
    struct stat buffer;
    if (stat(uri_.path.c_str(), &buffer) != 0)
    {
        throw SnapException("Failed to open PCM file: \"" + uri_.path + "\"");
    }
    else if ((buffer.st_mode & S_IFMT) != S_IFREG)
    {
        throw SnapException("Not a regular file: \"" + uri_.path + "\"");
    }
}


void FileStream::do_connect()
{
    LOG(DEBUG, LOG_TAG) << "connect\n";
    int fd = open(uri_.path.c_str(), O_RDONLY | O_NONBLOCK);
    stream_ = std::make_unique<boost::asio::posix::stream_descriptor>(ioc_, fd);
    on_connect();
}

} // namespace streamreader
