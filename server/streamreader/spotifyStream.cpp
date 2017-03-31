/***
    This file is part of snapcast
    Copyright (C) 2014-2017  Johannes Pohl

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
#include "common/utils.h"
#include "common/log.h"


using namespace std;




SpotifyStream::SpotifyStream(PcmListener* pcmListener, const StreamUri& uri) : ProcessStream(pcmListener, uri)
{
	sampleFormat_ = SampleFormat("44100:16:2");
 	uri_.query["sampleformat"] = sampleFormat_.getFormat();

	string username = uri_.getQuery("username", "");
	string password = uri_.getQuery("password", "");
	string bitrate = uri_.getQuery("bitrate", "320");
	string devicename = uri_.getQuery("devicename", "Snapcast");
	string onstart = uri_.getQuery("onstart", "");
	string onstop = uri_.getQuery("onstop", "");

	if (username.empty() != password.empty())
		throw SnapException("missing parameter \"username\" or \"password\" (must provide both, or neither)");

	params_ = "--name \"" + devicename + "\"";
	if (!username.empty() && !password.empty())
		params_ += " --username \"" + username + "\" --password \"" + password + "\"";
	params_ += " --bitrate " + bitrate + " --backend pipe";
	if (!onstart.empty())
		params_ += " --onstart \"" + onstart + "\"";
	if (!onstop.empty())
		params_ += " --onstop \"" + onstop + "\"";
//	logO << "params: " << params << "\n";
}


SpotifyStream::~SpotifyStream()
{
}


void SpotifyStream::initExeAndPath(const std::string& filename)
{
	path_ = "";
	exe_ = findExe(filename);
	if (!fileExists(exe_) || (exe_ == "/"))
	{
		exe_ = findExe("librespot");
		if (!fileExists(exe_))
			throw SnapException("librespot not found");
	}

	if (exe_.find("/") != string::npos)
	{
		path_ = exe_.substr(0, exe_.find_last_of("/") + 1);
		exe_ = exe_.substr(exe_.find_last_of("/") + 1);
	}

	/// kill if it's already running
	execGetOutput("killall " + exe_);
}


void SpotifyStream::onStderrMsg(const char* buffer, size_t n)
{
	/// Watch will kill librespot if there was no message received for 130min
	// 2016-11-02 22-05-15 [out] TRACE:librespot::stream: allocated stream 3580
	// 2016-11-02 22-05-15 [Debug] DEBUG:librespot::audio_file2: Got channel 3580
	// 2016-11-02 22-06-39 [out] DEBUG:librespot::spirc: kMessageTypeHello "SM-G901F" 5e1ffdd73f0d1741c4a173d5b238826464ca8e2f 1 0
	// 2016-11-02 22-06-39 [out] DEBUG:librespot::spirc: kMessageTypeNotify "Snapcast" 68724ecccd67781303655c49a73b74c5968667b1 123 1478120652755
	// 2016-11-02 22-06-40 [out] DEBUG:librespot::spirc: kMessageTypeNotify "SM-G901F" 5e1ffdd73f0d1741c4a173d5b238826464ca8e2f 1 0
	// 2016-11-02 22-06-41 [out] DEBUG:librespot::spirc: kMessageTypePause "SM-G901F" 5e1ffdd73f0d1741c4a173d5b238826464ca8e2f 2 0
	// 2016-11-02 22-06-42 [out] DEBUG:librespot::spirc: kMessageTypeNotify "Snapcast" 68724ecccd67781303655c49a73b74c5968667b1 124 1478120801615
	// 2016-11-02 22-06-47 [out] DEBUG:librespot::spirc: kMessageTypeNotify "SM-G901F" 5e1ffdd73f0d1741c4a173d5b238826464ca8e2f 2 1478120801615
	// 2016-11-02 22-35-10 [out] DEBUG:librespot::spirc: kMessageTypeNotify "Snapcast" 68724ecccd67781303655c49a73b74c5968667b1 125 1478120801615
	// 2016-11-02 23-36-06 [out] DEBUG:librespot::spirc: kMessageTypeNotify "Snapcast" 68724ecccd67781303655c49a73b74c5968667b1 126 1478120801615
	// 2016-11-03 01-37-08 [out] DEBUG:librespot::spirc: kMessageTypeNotify "Snapcast" 68724ecccd67781303655c49a73b74c5968667b1 127 1478120801615
	// 2016-11-03 02-38-13 [out] DEBUG:librespot::spirc: kMessageTypeNotify "Snapcast" 68724ecccd67781303655c49a73b74c5968667b1 128 1478120801615
	// killall librespot
	// 2016-11-03 09-00-18 [out] INFO:librespot::main_helper: librespot 6fa4e4d (2016-09-21). Built on 2016-10-27.
	// 2016-11-03 09-00-18 [out] INFO:librespot::session: Connecting to AP lon3-accesspoint-a34.ap.spotify.com:443
	// 2016-11-03 09-00-18 [out] INFO:librespot::session: Authenticated !
	watchdog_->trigger();
	string logmsg = trim_copy(string(buffer, n));
	if ((logmsg.find("allocated stream") == string::npos) &&
		(logmsg.find("Got channel") == string::npos) &&
		(logmsg.find('\0') == string::npos) &&
		(logmsg.size() > 4))
	{
		logO << "(" << getName() << ") " << logmsg << "\n";
	}
}


void SpotifyStream::stderrReader()
{
	watchdog_.reset(new Watchdog(this));
	/// 130min
	watchdog_->start(130*60*1000);
	ProcessStream::stderrReader();
}


void SpotifyStream::onTimeout(const Watchdog* watchdog, size_t ms)
{
	logE << "Spotify timeout: " << ms / 1000 << "\n";
	if (process_)
		process_->kill();
}


