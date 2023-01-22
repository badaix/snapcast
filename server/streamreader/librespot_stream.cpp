/***
    This file is part of snapcast
    Copyright (C) 2014-2023  Johannes Pohl

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

// prototype/interface header file
#include "librespot_stream.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/utils.hpp"
#include "common/utils/file_utils.hpp"
#include "common/utils/string_utils.hpp"

// standard headers
#include <exception>


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "LibrespotStream";

// https://commons.wikimedia.org/wiki/File:Spotify_logo_without_text.svg
static constexpr auto SPOTIFY_LOGO = "PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+Cjxz"
                                     "dmcgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIiBoZWlnaHQ9IjE2OHB4IiB3aWR0"
                                     "aD0iMTY4cHgiIHZlcnNpb249IjEuMSIgdmlld0JveD0iMCAwIDE2OCAxNjgiPgogPHBhdGggZmls"
                                     "bD0iIzFFRDc2MCIgZD0ibTgzLjk5NiAwLjI3N2MtNDYuMjQ5IDAtODMuNzQzIDM3LjQ5My04My43"
                                     "NDMgODMuNzQyIDAgNDYuMjUxIDM3LjQ5NCA4My43NDEgODMuNzQzIDgzLjc0MSA0Ni4yNTQgMCA4"
                                     "My43NDQtMzcuNDkgODMuNzQ0LTgzLjc0MSAwLTQ2LjI0Ni0zNy40OS04My43MzgtODMuNzQ1LTgz"
                                     "LjczOGwwLjAwMS0wLjAwNHptMzguNDA0IDEyMC43OGMtMS41IDIuNDYtNC43MiAzLjI0LTcuMTgg"
                                     "MS43My0xOS42NjItMTIuMDEtNDQuNDE0LTE0LjczLTczLjU2NC04LjA3LTIuODA5IDAuNjQtNS42"
                                     "MDktMS4xMi02LjI0OS0zLjkzLTAuNjQzLTIuODEgMS4xMS01LjYxIDMuOTI2LTYuMjUgMzEuOS03"
                                     "LjI5MSA1OS4yNjMtNC4xNSA4MS4zMzcgOS4zNCAyLjQ2IDEuNTEgMy4yNCA0LjcyIDEuNzMgNy4x"
                                     "OHptMTAuMjUtMjIuODA1Yy0xLjg5IDMuMDc1LTUuOTEgNC4wNDUtOC45OCAyLjE1NS0yMi41MS0x"
                                     "My44MzktNTYuODIzLTE3Ljg0Ni04My40NDgtOS43NjQtMy40NTMgMS4wNDMtNy4xLTAuOTAzLTgu"
                                     "MTQ4LTQuMzUtMS4wNC0zLjQ1MyAwLjkwNy03LjA5MyA0LjM1NC04LjE0MyAzMC40MTMtOS4yMjgg"
                                     "NjguMjIyLTQuNzU4IDk0LjA3MiAxMS4xMjcgMy4wNyAxLjg5IDQuMDQgNS45MSAyLjE1IDguOTc2"
                                     "di0wLjAwMXptMC44OC0yMy43NDRjLTI2Ljk5LTE2LjAzMS03MS41Mi0xNy41MDUtOTcuMjg5LTku"
                                     "Njg0LTQuMTM4IDEuMjU1LTguNTE0LTEuMDgxLTkuNzY4LTUuMjE5LTEuMjU0LTQuMTQgMS4wOC04"
                                     "LjUxMyA1LjIyMS05Ljc3MSAyOS41ODEtOC45OCA3OC43NTYtNy4yNDUgMTA5LjgzIDExLjIwMiAz"
                                     "LjczIDIuMjA5IDQuOTUgNy4wMTYgMi43NCAxMC43MzMtMi4yIDMuNzIyLTcuMDIgNC45NDktMTAu"
                                     "NzMgMi43Mzl6Ii8+Cjwvc3ZnPgo=";


LibrespotStream::LibrespotStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : ProcessStream(pcmListener, ioc, server_settings, uri)
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
    killall_ = (uri_.getQuery("killall", "false") == "true");

    if (username.empty() != password.empty())
        throw SnapException(R"(missing parameter "username" or "password" (must provide both, or neither))");

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
        params_ += " --autoplay on";
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
    if (!utils::file::exists(exe_) || (exe_ == "/"))
    {
        exe_ = findExe("librespot");
        if (!utils::file::exists(exe_))
            throw SnapException("librespot not found");
    }

    if (exe_.find('/') != string::npos)
    {
        path_ = exe_.substr(0, exe_.find_last_of('/') + 1);
        exe_ = exe_.substr(exe_.find_last_of('/') + 1);
    }

    if (killall_)
    {
        /// kill if it's already running
        execGetOutput("killall " + exe_);
    }
}

void LibrespotStream::onStderrMsg(const std::string& line)
{
    try
    {
        // Watch stderr for 'Loading track' messages and set the stream metadata
        // For more than track name check: https://github.com/plietar/librespot/issues/154

        /// Watch will kill librespot if there was no message received for 130min
        // 2021-05-09 09-25-48.651 [Info] (LibrespotStream) (Spotify) [2021-05-09T07:25:48Z DEBUG librespot_playback::player] command=Load(SpotifyId
        // 2021-05-09 09-25-48.651 [Info] (LibrespotStream) (Spotify) [2021-05-09T07:25:48Z TRACE librespot_connect::spirc] Sending status to server
        // 2021-05-09 09-25-48.746 [Info] (LibrespotStream) (Spotify) [2021-05-09T07:25:48Z WARN  librespot_connect::spirc] No autoplay_uri found
        // 2021-05-09 09-25-48.747 [Info] (LibrespotStream) (Spotify) [2021-05-09T07:25:48Z ERROR librespot_connect::spirc] AutoplayError: MercuryError
        // 2021-05-09 09-25-48.750 [Info] (LibrespotStream) (Spotify) [2021-05-09T07:25:48Z INFO  librespot_playback::player] Loading <Big Gangsta>

        // Parse log level, source and message from the log line
        // Format: [2021-05-09T08:31:08Z DEBUG librespot_playback::player] new Player[0]

        // std::cerr << "Librespot: " << line << "\n";
        std::string message = line;
        utils::string::split_left(message, ' ', message);
        std::string level = utils::string::trim_copy(utils::string::split_left(message, ' ', message));
        std::string source = utils::string::trim_copy(utils::string::split_left(message, ']', message));
        utils::string::trim(message);

        bool parsed = true;
        AixLog::Severity severity = AixLog::Severity::info;
        if (level == "TRACE")
            severity = AixLog::Severity::trace;
        else if (level == "DEBUG")
            severity = AixLog::Severity::debug;
        else if (level == "INFO")
            severity = AixLog::Severity::info;
        else if (level == "WARN")
            severity = AixLog::Severity::warning;
        else if (level == "ERROR")
            severity = AixLog::Severity::error;
        else
            parsed = false;

        if (parsed)
        {
            LOG(severity, source) << message << "\n";
            // std::cerr << "Parsed: " << "Severity: " << severity << ", source: " << source << ", msg: " << message << "\n";
        }
        else
        {
            return;
            // LOG(INFO, LOG_TAG) << "(" << getName() << ") " << line << "\n";
        }

        //  [2021-06-04T07:20:47Z INFO  librespot_playback::player] <Tunnel> (310573 ms) loaded
        // 	info!("Track \"{}\" loaded", track.name);
        size_t title_pos = 0;
        size_t ms_pos = 0;
        size_t n = 0;
        if (((title_pos = line.find("<")) != std::string::npos) && ((n = line.find(">", title_pos)) != std::string::npos) &&
            ((ms_pos = line.find("(", n)) != std::string::npos) && ((n = line.find("ms) loaded", ms_pos)) != std::string::npos))
        {
            title_pos += 1;
            std::string title = line.substr(title_pos, line.find(">", title_pos) - title_pos);
            LOG(INFO, LOG_TAG) << "metadata: <" << title << ">\n";
            ms_pos += 1;
            std::string ms = line.substr(ms_pos, n - ms_pos - 1);
            Metadata meta;
            meta.title = title;
            meta.duration = cpt::stod(ms) / 1000.;
            meta.art_data = {SPOTIFY_LOGO, "svg"};
            Properties properties;
            properties.metadata = std::move(meta);
            setProperties(properties);
        }
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Exception: " << e.what() << std::endl;
    }
}

} // namespace streamreader
