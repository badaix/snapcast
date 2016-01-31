/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#include "common/utils.h"
#include "streamManager.h"
#include "pipeReader.h"
#include "common/log.h"


using namespace std;


StreamManager::StreamManager(PcmListener* pcmListener, const std::string& defaultSampleFormat, const std::string& defaultCodec, size_t defaultReadBufferMs) : pcmListener_(pcmListener), sampleFormat_(defaultSampleFormat), codec_(defaultCodec), readBufferMs_(defaultReadBufferMs)
{
}


PcmReader* StreamManager::addStream(const std::string& uri)
{
	ReaderUri readerUri(uri);

	if (readerUri.query.find("sampleformat") == readerUri.query.end())
		readerUri.query["sampleformat"] = sampleFormat_;

	if (readerUri.query.find("codec") == readerUri.query.end())
		readerUri.query["codec"] = codec_;

	if (readerUri.query.find("buffer_ms") == readerUri.query.end())
		readerUri.query["buffer_ms"] = to_string(readBufferMs_);

//	logE << "\nURI: " << readerUri.uri << "\nscheme: " << readerUri.scheme << "\nhost: "
//		<< readerUri.host << "\npath: " << readerUri.path << "\nfragment: " << readerUri.fragment << "\n";

//	for (auto kv: readerUri.query)
//		logE << "key: '" << kv.first << "' value: '" << kv.second << "'\n";

	if (readerUri.scheme == "pipe")
	{
		streams_.push_back(make_shared<PipeReader>(pcmListener_, readerUri));//, sampleFormat, codec, pcmReadMs);
		return streams_.back().get();
	}

	return NULL;
}


const std::vector<PcmReaderPtr>& StreamManager::getStreams()
{
	return streams_;
}


const PcmReaderPtr StreamManager::getDefaultStream()
{
	if (streams_.empty())
		return nullptr;

	return streams_.front();
}


void StreamManager::start()
{
	for (auto stream: streams_)
		stream->start();
}


void StreamManager::stop()
{
	for (auto stream: streams_)
		stream->stop();
}


json StreamManager::toJson() const
{
	json result = json::array();
	for (auto stream: streams_)
		result.push_back(stream->getUri().toJson());
	return result;
}


