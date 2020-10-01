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

#include "librespot_stream.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/utils.hpp"
#include "common/utils/string_utils.hpp"
#include <regex>


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "LibrespotStream";


LibrespotStream::LibrespotStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : ProcessStream(pcmListener, ioc, uri)
{
    wd_timeout_sec_ = cpt::stoul(uri_.getQuery("wd_timeout", "7800")); ///< 130min

    string username = uri_.getQuery("username", "");
    string password = uri_.getQuery("password", "");
    string cache = uri_.getQuery("cache", "");
    bool disable_audio_cache = (uri_.getQuery("disable_audio_cache", "false") == "true");
    string volume = uri_.getQuery("volume", "100");
    string bitrate = uri_.getQuery("bitrate", "320");
    string devicename = uri_.getQuery("devicename", "Snapcast");
    string onevent = uri_.getQuery("onevent", "");
    bool normalize = (uri_.getQuery("normalize", "false") == "true");
    bool autoplay = (uri_.getQuery("autoplay", "false") == "true");
    killall_ = (uri_.getQuery("killall", "true") == "true");

    if (username.empty() != password.empty())
        throw SnapException("missing parameter \"username\" or \"password\" (must provide both, or neither)");

    if (!params_.empty())
        params_ += " ";
    params_ += "--name \"" + devicename + "\"";
    if (!username.empty() && !password.empty())
        params_ += " --username \"" + username + "\" --password \"" + password + "\"";
    params_ += " --bitrate " + bitrate + " --backend pipe";
    if (!cache.empty())
        params_ += " --cache \"" + cache + "\"";
    if (disable_audio_cache)
        params_ += " --disable-audio-cache";
    if (!volume.empty())
        params_ += " --initial-volume " + volume;
    if (!onevent.empty())
        params_ += " --onevent \"" + onevent + "\"";
    if (normalize)
        params_ += " --enable-volume-normalisation";
    if (autoplay)
        params_ += " --autoplay";
    params_ += " --verbose";

    if (uri_.query.find("username") != uri_.query.end())
        uri_.query["username"] = "xxx";
    if (uri_.query.find("password") != uri_.query.end())
        uri_.query["password"] = "xxx";
    //	LOG(INFO, LOG_TAG) << "params: " << params << "\n";
}


void LibrespotStream::initExeAndPath(const std::string& filename)
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

    if (killall_)
    {
        /// kill if it's already running
        execGetOutput("killall " + exe_);
    }
}


void LibrespotStream::onStderrMsg(const std::string& line)
{
    static bool libreelec_patched = false;
    smatch m;

    // Watch stderr for 'Loading track' messages and set the stream metadata
    // For more than track name check: https://github.com/plietar/librespot/issues/154

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

    if ((line.find("allocated stream") == string::npos) && (line.find("Got channel") == string::npos) && (line.find('\0') == string::npos) && (line.size() > 4))
    {
        LOG(INFO, LOG_TAG) << "(" << getName() << ") " << line << "\n";
    }

    // Librespot patch:
    // 	info!("metadata:{{\"ARTIST\":\"{}\",\"TITLE\":\"{}\"}}", artist.name, track.name);
    // non patched:
    // 	info!("Track \"{}\" loaded", track.name);

    // If we detect a patched libreelec we don't want to bother with this anymoer
    // to avoid duplicate metadata pushes
    if (!libreelec_patched)
    {
        static regex re_nonpatched("Track \"(.*)\" loaded");
        if (regex_search(line, m, re_nonpatched))
        {
            LOG(INFO, LOG_TAG) << "metadata: <" << m[1] << ">\n";

            json jtag = {{"TITLE", (string)m[1]}};
            setMeta(jtag);
        }
    }

    // Parse the patched version
    static regex re_patched("metadata:(.*)");
    if (regex_search(line, m, re_patched))
    {
        LOG(INFO, LOG_TAG) << "metadata: <" << m[1] << ">\n";

        setMeta(json::parse(m[1].str()));
        libreelec_patched = true;
    }
}

} // namespace streamreader
