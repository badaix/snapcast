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

#ifndef STREAM_MANAGER_HPP
#define STREAM_MANAGER_HPP

#include "pcm_stream.hpp"
#include <boost/asio/io_context.hpp>
#include <memory>
#include <string>
#include <vector>

namespace streamreader
{

typedef std::shared_ptr<PcmStream> PcmStreamPtr;

class StreamManager
{
public:
    StreamManager(PcmListener* pcmListener, boost::asio::io_context& ioc, const std::string& defaultSampleFormat, const std::string& defaultCodec,
                  size_t defaultChunkBufferMs = 20);

    PcmStreamPtr addStream(const std::string& uri);
    void removeStream(const std::string& name);
    void start();
    void stop();
    const std::vector<PcmStreamPtr>& getStreams();
    const PcmStreamPtr getDefaultStream();
    const PcmStreamPtr getStream(const std::string& id);
    json toJson() const;

private:
    std::vector<PcmStreamPtr> streams_;
    PcmListener* pcmListener_;
    std::string sampleFormat_;
    std::string codec_;
    size_t chunkBufferMs_;
    boost::asio::io_context& ioc_;
};

} // namespace streamreader

#endif
