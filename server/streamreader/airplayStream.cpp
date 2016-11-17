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
#include "common/utils.h"
#include "common/log.h"


using namespace std;




AirplayStream::AirplayStream(PcmListener* pcmListener, const StreamUri& uri) : ProcessStream(pcmListener, uri), port_(5000)
{
	logStderr_ = true;

	sampleFormat_ = SampleFormat("44100:16:2");
 	uri_.query["sampleformat"] = sampleFormat_.getFormat();

	port_ = cpt::stoul(uri_.getQuery("port", "5000")); 

	string devicename = uri_.getQuery("devicename", "Snapcast");
	params_wo_port_ = "--name=\"" + devicename + "\" --output=stdout";
	params_ = params_wo_port_ + " --port=" + cpt::to_string(port_);
}


AirplayStream::~AirplayStream()
{
}


void AirplayStream::initExeAndPath(const std::string& filename)
{
	path_ = "";
	exe_ = findExe(filename);
	if (!fileExists(exe_) || (exe_ == "/"))
	{
		exe_ = findExe("shairport-sync");
		if (!fileExists(exe_))
			throw SnapException("shairport-sync not found");
	}

	if (exe_.find("/") != string::npos)
	{
		path_ = exe_.substr(0, exe_.find_last_of("/") + 1);
		exe_ = exe_.substr(exe_.find_last_of("/") + 1);
	}
}


void AirplayStream::onStderrMsg(const char* buffer, size_t n)
{
	string logmsg = trim_copy(string(buffer, n));
	if (logmsg.empty())
		return;
	logO << "(" << getName() << ") " << logmsg << "\n";
	if (logmsg.find("Is another Shairport Sync running on this device") != string::npos)
	{
		logE << "Seem there is another Shairport Sync runnig on port " << port_ << ", switching to port " << port_ + 1 << "\n";
		++port_;
		params_ = params_wo_port_ + " --port=" + cpt::to_string(port_);
	}
	else if (logmsg.find("Invalid audio output specified") != string::npos)
	{
		logE << "shairport sync compiled without stdout audio backend\n";
		logE << "build with: \"./configure --with-stdout --with-avahi --with-ssl=openssl --with-metadata\"\n";
	}
}

