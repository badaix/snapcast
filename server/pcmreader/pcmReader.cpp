/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#include <memory>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pcmReader.h"
#include "../encoder/encoderFactory.h"
#include "common/utils.h"
#include "common/snapException.h"
#include "common/log.h"


using namespace std;


ReaderUri::ReaderUri(const std::string& uri)
{
// https://en.wikipedia.org/wiki/Uniform_Resource_Identifier
// scheme:[//[user:password@]host[:port]][/]path[?query][#fragment]
	size_t pos;
	this->uri = uri;
	string tmp(uri);

	pos = tmp.find(':');
	if (pos == string::npos)
		throw invalid_argument("missing ':'");
	scheme = tmp.substr(0, pos);
	tmp = tmp.substr(pos + 1);
//	logD << "scheme: '" << scheme << "' tmp: '" << tmp << "'\n";

	if (tmp.find("//") != 0)
		throw invalid_argument("missing host separator: '//'");
	tmp = tmp.substr(2);

	pos = tmp.find('/');
	if (pos == string::npos)
		throw invalid_argument("missing path separator: '/'");
	host = tmp.substr(0, pos);
	tmp = tmp.substr(pos + 1);
	path = tmp;
//	logD << "host: '" << host << "' tmp: '" << tmp << "' path: '" << path << "'\n";

	pos = tmp.find('?');
	if (pos == string::npos)
		return;

	path = tmp.substr(0, pos);
	tmp = tmp.substr(pos + 1);
	string queryStr = tmp;
//	logD << "path: '" << path << "' tmp: '" << tmp << "' query: '" << queryStr << "'\n";

	pos = tmp.find('#');
	if (pos != string::npos)
	{
		queryStr = tmp.substr(0, pos);
		tmp = tmp.substr(pos + 1);
		fragment = tmp;
//		logD << "query: '" << queryStr << "' fragment: '" << fragment << "' tmp: '" << tmp << "'\n";
	}

	vector<string> keyValueList = split(queryStr, '&');
	for (auto& kv: keyValueList)
	{
		pos = kv.find('=');
		if (pos != string::npos)
			query[kv.substr(0, pos)] = kv.substr(pos+1);
	}

//	for (auto& kv: query)
//		logD << "key: '" << kv.first << "' value: '" << kv.second << "'\n";
}


PcmReader::PcmReader(PcmListener* pcmListener, const msg::SampleFormat& sampleFormat, const std::string& codec, const std::string& fifoName, size_t pcmReadMs) : pcmListener_(pcmListener), sampleFormat_(sampleFormat), pcmReadMs_(pcmReadMs)
{
	EncoderFactory encoderFactory;
	encoder_.reset(encoderFactory.createEncoder(codec));
}


PcmReader::~PcmReader()
{
	stop();
}


msg::Header* PcmReader::getHeader()
{
	return encoder_->getHeader();
}


void PcmReader::start()
{
	encoder_->init(this, sampleFormat_);
 	active_ = true;
	readerThread_ = thread(&PcmReader::worker, this);
}


void PcmReader::stop()
{
	if (active_)
	{
		active_ = false;
		readerThread_.join();
	}
}


void PcmReader::onChunkEncoded(const Encoder* encoder, msg::PcmChunk* chunk, double duration)
{
//	logO << "onChunkEncoded: " << duration << " us\n";
	if (duration <= 0)
		return;

	chunk->timestamp.sec = tvEncodedChunk_.tv_sec;
	chunk->timestamp.usec = tvEncodedChunk_.tv_usec;
	chronos::addUs(tvEncodedChunk_, duration * 1000);
	pcmListener_->onChunkRead(this, chunk, duration);
}

