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

#pragma once


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

/// Shared pointer to a stream object
using PcmStreamPtr = std::shared_ptr<PcmStream>;

/// Manage all available stream sources
class StreamManager
{
public:
    /// C'tor
    StreamManager(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& settings);

    /// Construct and add a stream from @p uri
    /// @return the created stream
    PcmStreamPtr addStream(const std::string& uri);
    /// Construct and add a stream from @p streamUri
    /// @return the created stream
    PcmStreamPtr addStream(StreamUri& streamUri);
    /// Remove a stream by @p name
    void removeStream(const std::string& name);

    /// Start all stream sources, i.e the streams sources will start reading their respective inputs
    void start();
    /// Stop all stream sources
    void stop();

    /// @return list of all available streams
    const std::vector<PcmStreamPtr>& getStreams() const;
    /// @return default stream for groups that don't have a stream source configured
    const PcmStreamPtr getDefaultStream() const;
    /// @return stream by id (id is an alias for name)
    const PcmStreamPtr getStream(const std::string& id) const;
    /// @return all streams with details as json
    json toJson() const;

private:
    std::vector<PcmStreamPtr> streams_;
    PcmStream::Listener* pcmListener_;
    ServerSettings settings_;
    boost::asio::io_context& io_context_;
};

} // namespace streamreader
