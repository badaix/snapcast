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

#include "stream_manager.hpp"
#include "airplay_stream.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils.hpp"
#include "file_stream.hpp"
#include "librespot_stream.hpp"
#include "pipe_stream.hpp"
#include "process_stream.hpp"
#include "tcp_stream.hpp"


using namespace std;

namespace streamreader
{

StreamManager::StreamManager(PcmListener* pcmListener, boost::asio::io_context& ioc, const std::string& defaultSampleFormat, const std::string& defaultCodec,
                             size_t defaultChunkBufferMs)
    : pcmListener_(pcmListener), sampleFormat_(defaultSampleFormat), codec_(defaultCodec), chunkBufferMs_(defaultChunkBufferMs), ioc_(ioc)
{
}


PcmStreamPtr StreamManager::addStream(const std::string& uri)
{
    StreamUri streamUri(uri);

    if (streamUri.query.find(kUriSampleFormat) == streamUri.query.end())
        streamUri.query[kUriSampleFormat] = sampleFormat_;

    if (streamUri.query.find(kUriCodec) == streamUri.query.end())
        streamUri.query[kUriCodec] = codec_;

    if (streamUri.query.find(kUriChunkMs) == streamUri.query.end())
        streamUri.query[kUriChunkMs] = cpt::to_string(chunkBufferMs_);

    //	LOG(DEBUG) << "\nURI: " << streamUri.uri << "\nscheme: " << streamUri.scheme << "\nhost: "
    //		<< streamUri.host << "\npath: " << streamUri.path << "\nfragment: " << streamUri.fragment << "\n";

    //	for (auto kv: streamUri.query)
    //		LOG(DEBUG) << "key: '" << kv.first << "' value: '" << kv.second << "'\n";
    PcmStreamPtr stream(nullptr);

    if (streamUri.scheme == "pipe")
    {
        stream = make_shared<PipeStream>(pcmListener_, ioc_, streamUri);
    }
    else if (streamUri.scheme == "file")
    {
        stream = make_shared<FileStream>(pcmListener_, ioc_, streamUri);
    }
    else if (streamUri.scheme == "process")
    {
        stream = make_shared<ProcessStream>(pcmListener_, ioc_, streamUri);
    }
    else if ((streamUri.scheme == "spotify") || (streamUri.scheme == "librespot"))
    {
        // Overwrite sample format here instead of inside the constructor, to make sure
        // that all constructors of all parent classes also use the overwritten sample
        // format.
        streamUri.query[kUriSampleFormat] = "44100:16:2";
        stream = make_shared<LibrespotStream>(pcmListener_, ioc_, streamUri);
    }
    else if (streamUri.scheme == "airplay")
    {
        // Overwrite sample format here instead of inside the constructor, to make sure
        // that all constructors of all parent classes also use the overwritten sample
        // format.
        streamUri.query[kUriSampleFormat] = "44100:16:2";
        stream = make_shared<AirplayStream>(pcmListener_, ioc_, streamUri);
    }
    else if (streamUri.scheme == "tcp")
    {
        stream = make_shared<TcpStream>(pcmListener_, ioc_, streamUri);
    }
    else
    {
        throw SnapException("Unknown stream type: " + streamUri.scheme);
    }

    if (stream)
    {
        for (const auto& s : streams_)
        {
            if (s->getName() == stream->getName())
                throw SnapException("Stream with name \"" + stream->getName() + "\" already exists");
        }
        streams_.push_back(stream);
    }

    return stream;
}


void StreamManager::removeStream(const std::string& name)
{
    auto iter = std::find_if(streams_.begin(), streams_.end(), [&name](const PcmStreamPtr& stream) { return stream->getName() == name; });
    if (iter != streams_.end())
    {
        (*iter)->stop();
        streams_.erase(iter);
    }
}


const std::vector<PcmStreamPtr>& StreamManager::getStreams()
{
    return streams_;
}


const PcmStreamPtr StreamManager::getDefaultStream()
{
    if (streams_.empty())
        return nullptr;

    return streams_.front();
}


const PcmStreamPtr StreamManager::getStream(const std::string& id)
{
    for (auto stream : streams_)
    {
        if (stream->getId() == id)
            return stream;
    }
    return nullptr;
}


void StreamManager::start()
{
    for (const auto& stream : streams_)
        stream->start();
}


void StreamManager::stop()
{
    for (const auto& stream : streams_)
        if (stream)
            stream->stop();
}


json StreamManager::toJson() const
{
    json result = json::array();
    for (auto stream : streams_)
        result.push_back(stream->toJson());
    return result;
}

} // namespace streamreader
