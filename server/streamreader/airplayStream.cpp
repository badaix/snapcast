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

#include "airplayStream.h"
#include "common/snapException.h"


using namespace std;




AirplayStream::AirplayStream(PcmListener* pcmListener, const StreamUri& uri) : ProcessStream(pcmListener, uri)
{
	logStderr_ = true;
	
	sampleFormat_ = SampleFormat("44100:16:2");
 	uri_.query["sampleformat"] = sampleFormat_.getFormat();

	string devicename = uri_.getQuery("devicename", "Snapcast");
	params_ = "--name=\"" + devicename + "\" --output=stdout";
}


AirplayStream::~AirplayStream()
{
}


void AirplayStream::initExeAndPath(const std::string& filename)
{
	exe_ = findExe(filename);
	if (!fileExists(exe_) || (exe_ == "/"))
	{
		exe_ = findExe("shairport-sync");
		if (!fileExists(exe_))
			throw SnapException("shairport-sync not found");
	}

	if (exe_.find("/") != string::npos)
		path_ = exe_.substr(0, exe_.find_last_of("/"));
}

