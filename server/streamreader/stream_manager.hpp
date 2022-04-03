/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

// local headers
#include "pcm_stream.hpp"
#include "server_settings.hpp"

// 3rd party headers
#include <boost/asio/io_context.hpp>

// standard headers
#include <memory>
#include <string>
#include <vector>

namespace streamreader
{

using PcmStreamPtr = std::shared_ptr<PcmStream>;

class StreamManager
{
public:
    StreamManager(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& settings);

    PcmStreamPtr addStream(const std::string& uri);
    PcmStreamPtr addStream(StreamUri& streamUri);
    void removeStream(const std::string& name);
    void start();
    void stop();
    const std::vector<PcmStreamPtr>& getStreams();
    const PcmStreamPtr getDefaultStream();
    const PcmStreamPtr getStream(const std::string& id);
    json toJson() const;

private:
    std::vector<PcmStreamPtr> streams_;
    PcmStream::Listener* pcmListener_;
    ServerSettings settings_;
    boost::asio::io_context& io_context_;
};

} // namespace streamreader

#endif
