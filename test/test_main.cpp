/***
    This file is part of snapcast
    Copyright (C) 2014-2021  Johannes Pohl

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

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <regex>

#include "common/aixlog.hpp"
#include "common/utils/string_utils.hpp"
#include "server/streamreader/control_error.hpp"
#include "server/streamreader/properties.hpp"
#include "server/streamreader/stream_uri.hpp"

using namespace std;


TEST_CASE("String utils")
{
    using namespace utils::string;
    REQUIRE(ltrim_copy(" test") == "test");
}


TEST_CASE("Uri")
{
    AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::debug);
    using namespace streamreader;
    StreamUri uri("pipe:///tmp/snapfifo?name=default&codec=flac");
    REQUIRE(uri.scheme == "pipe");
    REQUIRE(uri.path == "/tmp/snapfifo");
    REQUIRE(uri.host.empty());

    // uri = StreamUri("scheme:[//host[:port]][/]path[?query=none][#fragment]");
    // Test with all fields
    uri = StreamUri("scheme://host:port/path?query=none&key=value#fragment");
    REQUIRE(uri.scheme == "scheme");
    REQUIRE(uri.host == "host:port");
    REQUIRE(uri.path == "/path");
    REQUIRE(uri.query["query"] == "none");
    REQUIRE(uri.query["key"] == "value");
    REQUIRE(uri.fragment == "fragment");

    // Test with all fields, url encoded
    // "%21%23%24%25%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"
    // "!#$%&'()*+,/:;=?@[]"
    uri = StreamUri("scheme%26://%26host%3f:port/pa%2Bth?%21%23%24%25%26%27%28%29=%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D&key%2525=value#fragment%3f%21%3F");
    REQUIRE(uri.scheme == "scheme&");
    REQUIRE(uri.host == "&host?:port");
    REQUIRE(uri.path == "/pa+th");
    REQUIRE(uri.query["!#$%&'()"] == "*+,/:;=?@[]");
    REQUIRE(uri.query["key%25"] == "value");
    REQUIRE(uri.fragment == "fragment?!?");

    // No host
    uri = StreamUri("scheme:///path?query=none#fragment");
    REQUIRE(uri.scheme == "scheme");
    REQUIRE(uri.path == "/path");
    REQUIRE(uri.query["query"] == "none");
    REQUIRE(uri.fragment == "fragment");

    // No host, no query
    uri = StreamUri("scheme:///path#fragment");
    REQUIRE(uri.scheme == "scheme");
    REQUIRE(uri.path == "/path");
    REQUIRE(uri.query.empty());
    REQUIRE(uri.fragment == "fragment");

    // No host, no fragment
    uri = StreamUri("scheme:///path?query=none");
    REQUIRE(uri.scheme == "scheme");
    REQUIRE(uri.path == "/path");
    REQUIRE(uri.query["query"] == "none");
    REQUIRE(uri.fragment.empty());

    // just schema and path
    uri = StreamUri("scheme:///path");
    REQUIRE(uri.scheme == "scheme");
    REQUIRE(uri.path == "/path");
    REQUIRE(uri.query.empty());
    REQUIRE(uri.fragment.empty());

    // Issue #850
    uri = StreamUri("spotify:///librespot?name=Spotify&username=EMAIL&password=string%26with%26ampersands&devicename=Snapcast&bitrate=320&killall=false");
    REQUIRE(uri.scheme == "spotify");
    REQUIRE(uri.host.empty());
    REQUIRE(uri.path == "/librespot");
    REQUIRE(uri.query["name"] == "Spotify");
    REQUIRE(uri.query["username"] == "EMAIL");
    REQUIRE(uri.query["password"] == "string&with&ampersands");
    REQUIRE(uri.query["devicename"] == "Snapcast");
    REQUIRE(uri.query["bitrate"] == "320");
    REQUIRE(uri.query["killall"] == "false");
}


TEST_CASE("Metadata")
{
    auto in_json = json::parse(R"(
{
    "album": "Memories...Do Not Open",
    "albumArtist": [
        "The Chainsmokers"
    ],
    "albumArtistSort": [
        "Chainsmokers, The"
    ],
    "artist": [
        "The Chainsmokers & Coldplay"
    ],
    "artistSort": [
        "Chainsmokers, The & Coldplay"
    ],
    "contentCreated": "2017-04-07",
    "discNumber": 1,
    "duration": 247.0,
    "file": "The Chainsmokers - Memories...Do Not Open (2017)/05 - Something Just Like This.mp3",
    "genre": [
        "Dance/Electronic"
    ],
    "label": "Columbia/Disruptor Records",
    "musicbrainzAlbumArtistId": "91a81925-92f9-4fc9-b897-93cf01226282",
    "musicbrainzAlbumId": "a9ff33d7-c5a1-4e15-83f7-f669ff96c196",
    "musicbrainzArtistId": "91a81925-92f9-4fc9-b897-93cf01226282/cc197bad-dc9c-440d-a5b5-d52ba2e14234",
    "musicbrainzReleaseTrackId": "8ccd52a8-de1d-48ce-acf9-b382a7296cfe",
    "musicbrainzTrackId": "6fdd95d6-9837-4d95-91f4-b123d0696a2a",
    "originalDate": "2017",
    "title": "Something Just Like This",
    "trackNumber": 5
}
)");
    // std::cout << in_json.dump(4) << "\n";

    Metadata meta(in_json);
    REQUIRE(meta.album.has_value());
    REQUIRE(meta.album.value() == "Memories...Do Not Open");
    REQUIRE(meta.genre.has_value());
    REQUIRE(meta.genre->size() == 1);
    REQUIRE(meta.genre.value().front() == "Dance/Electronic");

    auto out_json = meta.toJson();
    // std::cout << out_json.dump(4) << "\n";
    REQUIRE(in_json == out_json);
}


TEST_CASE("Properties")
{

    std::stringstream ss;
    ss << PlaybackStatus::kPlaying;
    REQUIRE(ss.str() == "playing");

    PlaybackStatus playback_status;
    ss >> playback_status;
    REQUIRE(playback_status == PlaybackStatus::kPlaying);

    REQUIRE(to_string(PlaybackStatus::kPaused) == "paused");
    auto in_json = json::parse(R"(
{
    "playbackStatus": "playing",
    "loopStatus": "track",
    "shuffle": false,
    "volume": 42,
    "position": 23.0
}
)");
    // std::cout << in_json.dump(4) << "\n";

    Properties properties(in_json);
    std::cout << properties.toJson().dump(4) << "\n";

    REQUIRE(properties.loop_status.has_value());

    auto out_json = properties.toJson();
    // std::cout << out_json.dump(4) << "\n";
    REQUIRE(in_json == out_json);

    in_json = json::parse(R"(
{
    "volume": 42
}
)");
    // std::cout << in_json.dump(4) << "\n";

    properties.fromJson(in_json);
    // std::cout << properties.toJson().dump(4) << "\n";

    REQUIRE(!properties.loop_status.has_value());

    out_json = properties.toJson();
    // std::cout << out_json.dump(4) << "\n";
    REQUIRE(in_json == out_json);
}


TEST_CASE("Librespot")
{
    std::string line = "[2021-06-04T07:20:47Z INFO  librespot_playback::player] <Tunnel> (310573 ms) loaded";

    smatch m;
    static regex re_track_loaded(R"( <(.*)> \((.*) ms\) loaded)");
    // Parse the patched version
    if (std::regex_search(line, m, re_track_loaded))
    {
        REQUIRE(m.size() == 3);
        REQUIRE(m[1] == "Tunnel");
        REQUIRE(m[2] == "310573");
    }
    REQUIRE(m.size() == 3);

    static regex re_log_line(R"(\[(\S+)(\s+)(\bTRACE\b|\bDEBUG\b|\bINFO\b|\bWARN\b|\bERROR\b)(\s+)(.*)\] (.*))");
    // Parse the patched version
    REQUIRE(std::regex_search(line, m, re_log_line));
    REQUIRE(m.size() == 7);
    REQUIRE(m[1] == "2021-06-04T07:20:47Z");
    REQUIRE(m[2] == " ");
    REQUIRE(m[3] == "INFO");
    REQUIRE(m[4] == "  ");
    REQUIRE(m[5] == "librespot_playback::player");
    REQUIRE(m[6] == "<Tunnel> (310573 ms) loaded");
    for (const auto& match : m)
        std::cerr << "Match: '" << match << "'\n";
}


TEST_CASE("Error")
{
    std::error_code ec = ControlErrc::can_not_control;
    REQUIRE(ec);
    REQUIRE(ec == ControlErrc::can_not_control);
    REQUIRE(ec != ControlErrc::success);
    std::cout << ec << std::endl;

    ec = make_error_code(ControlErrc::can_not_control);
    REQUIRE(ec.category() == snapcast::error::control::category());
    std::cout << "Category: " << ec.category().name() << ", " << ec.message() << std::endl;
}
