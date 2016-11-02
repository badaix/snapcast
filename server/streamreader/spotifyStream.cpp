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

#include "spotifyStream.h"
#include "common/snapException.h"
#include "common/log.h"


using namespace std;




SpotifyStream::SpotifyStream(PcmListener* pcmListener, const StreamUri& uri) : ProcessStream(pcmListener, uri)
{
	sampleFormat_ = SampleFormat("44100:16:2");
	string username = uri_.getQuery("username", "");
	string password = uri_.getQuery("password", "");
	string bitrate = uri_.getQuery("bitrate", "320");
	string devicename = uri_.getQuery("devicename", "Snapcast");

	if (username.empty())
		throw SnapException("missing parameter \"username\"");
	if (password.empty())
		throw SnapException("missing parameter \"password\"");

	params_ = "--name \"" + devicename + "\" --username \"" + username + "\" --password \"" + password + "\" --bitrate " + bitrate + " --backend stdout";
//	logO << "params: " << params << "\n";
}


SpotifyStream::~SpotifyStream()
{
}


void SpotifyStream::initExeAndPath(const std::string& filename)
{
	exe_ = findExe(filename);
	if (!fileExists(exe_) || (exe_ == "/"))
	{
		exe_ = findExe("librespot");
		if (!fileExists(exe_))
			throw SnapException("librespot not found");
	}

	if (exe_.find("/") != string::npos)
		path_ = exe_.substr(0, exe_.find_last_of("/"));
}


void SpotifyStream::onStderrMsg(const char* buffer, size_t n)
{
	string logmsg(buffer, n);
	if ((logmsg.find("allocated stream") == string::npos) &&
		(logmsg.find("Got channel") == string::npos) &&
		(logmsg.size() > 4))
	{
		logO << logmsg;
	}
}

