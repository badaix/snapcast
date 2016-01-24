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
#include "pcmReaderFactory.h"
#include "pipeReader.h"
#include "common/log.h"


using namespace std;


PcmReader* PcmReaderFactory::createPcmReader(PcmListener* pcmListener, const std::string& uri, const std::string& defaultSampleFormat, const std::string& defaultCodec, size_t defaultReadBufferMs)
{
	ReaderUri readerUri(uri);

	if (readerUri.query.find("sampleformat") == readerUri.query.end())
		readerUri.query["sampleformat"] = defaultSampleFormat;

	if (readerUri.query.find("codec") == readerUri.query.end())
		readerUri.query["codec"] = defaultCodec;

	logE << "\nURI: " << readerUri.uri << "\nscheme: " << readerUri.scheme << "\nhost: "
		<< readerUri.host << "\npath: " << readerUri.path << "\nfragment: " << readerUri.fragment << "\n";

	for (auto kv: readerUri.query)
		logE << "key: '" << kv.first << "' value: '" << kv.second << "'\n";

	if (readerUri.scheme == "pipe")
		return new PipeReader(pcmListener, readerUri);//, sampleFormat, codec, pcmReadMs);

	return NULL;
}



