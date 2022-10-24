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

// prototype/interface header file
#include "catch.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/utils/string_utils.hpp"
#include "server/streamreader/control_error.hpp"
#include "server/streamreader/properties.hpp"
#include "server/streamreader/stream_uri.hpp"

// 3rd party headers

// standard headers
#include <regex>


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
    "url": "The Chainsmokers - Memories...Do Not Open (2017)/05 - Something Just Like This.mp3",
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
    "canControl": false,
    "canGoNext": false,
    "canGoPrevious": false,
    "canPause": false,
    "canPlay": false,
    "canSeek": false,
    "playbackStatus": "playing",
    "loopStatus": "track",
    "shuffle": false,
    "volume": 42,
    "mute": false,
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
    "canControl": true,
    "canGoNext": true,
    "canGoPrevious": true,
    "canPause": true,
    "canPlay": true,
    "canSeek": true,
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


TEST_CASE("Librespot2")
{
    std::string line = "[2021-06-04T07:20:47Z INFO  librespot_playback::player] <Tunnel> (310573 ms) loaded";
    size_t n = 0;
    size_t title_pos = 0;
    size_t ms_pos = 0;
    if (((title_pos = line.find("<")) != std::string::npos) && ((n = line.find(">", title_pos)) != std::string::npos) &&
        ((ms_pos = line.find("(", n)) != std::string::npos) && ((n = line.find("ms) loaded", ms_pos)) != std::string::npos))
    {
        title_pos += 1;
        std::string title = line.substr(title_pos, line.find(">", title_pos) - title_pos);
        REQUIRE(title == "Tunnel");
        ms_pos += 1;
        std::string ms = line.substr(ms_pos, n - ms_pos - 1);
        REQUIRE(ms == "310573");
    }

    line = "[2021-06-04T07:20:47Z INFO  librespot_playback::player] metadata:{\"ARTIST\":\"artist\",\"TITLE\":\"title\"}";
    n = 0;
    if (((n = line.find("metadata:")) != std::string::npos))
    {
        std::string meta = line.substr(n + 9);
        REQUIRE(meta == "{\"ARTIST\":\"artist\",\"TITLE\":\"title\"}");
        json j = json::parse(meta);
        std::string artist = j["ARTIST"].get<std::string>();
        REQUIRE(artist == "artist");
        std::string title = j["TITLE"].get<std::string>();
        REQUIRE(title == title);
    }
}


// TEST_CASE("Librespot2")
// {
//     std::vector<std::string>
//         loglines =
//             {R"xx([2022-06-27T20:44:48Z INFO  librespot] librespot 0.4.1 c8897dd332 (Built on 2022-05-24, Build ID: 1653416940, Profile: release))xx",
//              R"xx([2022-06-27T20:44:48Z TRACE librespot] Command line argument(s):)xx",
//              R"xx([2022-06-27T20:44:48Z TRACE librespot] 		-n "libre")xx",
//              R"xx([2022-06-27T20:44:48Z TRACE librespot] 		-b "160")xx",
//              R"xx([2022-06-27T20:44:48Z TRACE librespot] 		-v)xx",
//              R"xx([2022-06-27T20:44:48Z TRACE librespot] 		--backend "pipe")xx",
//              R"xx([2022-06-27T20:44:48Z TRACE librespot] 		--device "/dev/null")xx",
//              R"xx([2022-06-27T20:44:48Z DEBUG librespot_discovery::server] Zeroconf server listening on 0.0.0.0:46693)xx",
//              R"xx([2022-06-27T20:44:59Z DEBUG librespot_discovery::server] POST "/" {})xx",
//              R"xx([2022-06-27T20:44:59Z INFO  librespot_core::session] Connecting to AP "ap-gae2.spotify.com:4070")xx",
//              R"xx([2022-06-27T20:45:00Z INFO  librespot_core::session] Authenticated as "REDACTED" !)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_core::session] new Session[0])xx",
//              R"xx([2022-06-27T20:45:00Z INFO  librespot_playback::mixer::softmixer] Mixing with softvol and volume control: Log(60.0))xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_connect::spirc] new Spirc[0])xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_connect::spirc] canonical_username: REDACTED)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot::component] new MercuryManager)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_playback::player] new Player[0])xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_playback::mixer::mappings] Input volume 58958 mapped to: 49.99%)xx",
//              R"xx([2022-06-27T20:45:00Z INFO  librespot_playback::convert] Converting with ditherer: tpdf)xx",
//              R"xx([2022-06-27T20:45:00Z INFO  librespot_playback::audio_backend::pipe] Using pipe sink with format: S16)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_playback::player] command=AddEventSender)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_playback::player] command=VolumeSet(58958))xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_core::session] Session[0] strong=3 weak=2)xx",
//              R"xx([2022-06-27T20:45:00Z INFO  librespot_core::session] Country: "NZ")xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_core::mercury] subscribed uri=hm://remote/user/REDACTED/ count=0)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_connect::spirc] kMessageTypeNotify "REDACTED’s MacBook Air" 77fff453997b1fd01bfe33e60acca5b91948f3df
//              652808319 1656362700156 kPlayStatusStop)xx", R"xx([2022-06-27T20:45:00Z DEBUG librespot_connect::spirc] kMessageTypeNotify "REDACTED"
//              73d40aa67ebaebab0f887eae39a3dca1a29a68f0 652808319 1656362700156 kPlayStatusStop)xx", R"xx([2022-06-27T20:45:00Z DEBUG librespot_connect::spirc]
//              kMessageTypeLoad "REDACTED MacBook Air" 77fff453997b1fd01bfe33e60acca5b91948f3df 652808631 1656362700156 kPlayStatusPause)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_connect::spirc] State: context_uri: "spotify:playlist:6C0FrjLUZZ99ovwSXwt0DE" index: 10 position_ms:
//              126126 status: kPlayStatusPause position_measured_at: 1656362700511 context_description: "" shuffle: false repeat: false playing_from_fallback:
//              true row: 0 playing_track_index: 10 track {gid: "\315i\201.\177BC\316\223\302h!|\203\264\024"} track {gid:
//              "y\336[{\220\237C\022\210\300\032_\246\276Eu"} track {gid: "\323.H^\265\265A\313\265\'\205\220\272*\341\030"} track {gid:
//              ")\224E\314)\357GX\201\277\311l\277\364`$"} track {gid: "\310#\223\310\254\376H\026\214\3600\r\201\000R\313"} track {gid:
//              "\223\227#\3447\354M\322\234\222\355\242Kpq)"} track {gid: "\326\326\235\t\320\244K<\231\336\032V\240d%\031"} track {gid:
//              "\007\204\252o\022\303N\200\214\377\221\310EW7\230"} track {gid: "\327\307\336\265koLt\2507a9\364\306}j"} track {gid:
//              ")\221-\034\316`J\303\241\362\020\335\221\034:\320"} track {gid: "M\332\300\341\260\264M\340\216\237[\3313<\221 "} track {gid:
//              "\202q\273\177i\352J.\212\222\335\303\002a!z"} track {gid: ",\356\372\\\215D@j\203\366\022\021&\336\237\000"} track {gid:
//              "<\031\235{\305\320O-\264a\331=\273\236\365\220"} track {gid: "\316[\002\222\3704J*\2157V\336\344\266\351\314"} track {gid:
//              "1+\360\n;\212Nb\235%\2476xL\257\265"} track {gid: "j\207K\250f<L\337\241|C\3376w-\203"} track {gid: "\361$p\035\221\377H
//              \216\216-\265\000\211f)"} track {gid: "\\N9h\211.Jz\236\013`\202\033\230i\313"} track {gid: ".\036\211\302\247\234C\336\252\353>\034\306RcH"}
//              track {gid: ";wr\260\264\234E>\207\200\301n9\261\022w"} track {gid: "\010\226\300\377BDD\377\215\030\376\013j\311\262*"} track {gid:
//              "\377\272\345m\324\232N\321\203\253I\016I\321H\315"} track {gid: "\313\336\321<c\350N\005\265\037\354b\330w\240&"} track {gid:
//              "z\024_I\213\335O\002\264kiu\331\317\n\375"} track {gid: "%/\230z\337JK+\2027\253\021\300b\370\214"} track {gid: "\016\257\036\312\364\336H
//              \252\216\376%\343\2778|"} track {gid: "\013\323\303Y\261\002J\330\236\272zT\017\230\200\004"} track {gid:
//              "NN~u\246\177M\273\224T\335y\312&m\027"} track {gid: "mq\000\314&\267M\226\222[\350(\230\321\211\315"} track {gid:
//              "\344\361\314\222\371\023J~\275\345M@M\0214\000"})xx", R"xx([2022-06-27T20:45:00Z DEBUG librespot_connect::spirc] Frame has 31 tracks)xx",
//              R"xx([2022-06-27T20:45:00Z TRACE librespot_connect::spirc] Sending status to server: [kPlayStatusPause])xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_playback::player] command=SetAutoNormaliseAsAlbum(false))xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_playback::player] command=Load(SpotifyId { id: REDACTED, audio_type: Track }, false, 126126))xx",
//              R"xx([2022-06-27T20:45:00Z TRACE librespot_connect::spirc] Sending status to server: [kPlayStatusPause])xx",
//              R"xx([2022-06-27T20:45:00Z INFO  librespot_playback::player] Loading <Flip Reset> with Spotify URI <spotify:track:2mUnxSunvs3Clvtpq3FtGo>)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot_audio::fetch] Downloading file REDACTED)xx",
//              R"xx([2022-06-27T20:45:00Z DEBUG librespot::component] new ChannelManager)xx",
//              R"xx([2022-06-27T20:45:01Z DEBUG librespot::component] new AudioKeyManager)xx",
//              R"xx([2022-06-27T20:45:03Z INFO  librespot_playback::player] <Flip Reset> (187661 ms) loaded)xx",
//              R"xx([2022-06-27T20:45:03Z TRACE librespot_connect::spirc] Sending status to server: [kPlayStatusPause])xx",
//              R"xx([2022-06-27T20:45:03Z TRACE librespot_connect::spirc] ==> kPlayStatusPause)xx",
//              R"xx([2022-06-30T15:25:37Z DEBUG librespot_connect::spirc] State: context_uri: "spotify:show:4bGWMQA1ANGTgcysDo14aX" index: 10 position_ms:
//              3130170 status: kPlayStatusPause position_measured_at: 1656602737490 context_description: "" shuffle: false repeat: false playing_from_fallback:
//              true row: 0 playing_track_index: 10 track {uri: "spotify:episode:7luHEaQvEf2GA241SqaoIg"} track {uri: "spotify:episode:29LOC9vr1vWakVpWBwuF7n"}
//              track {uri: "spotify:episode:2MS5phBmztGepq6yft07XA"} track {uri: "spotify:episode:1kqqpitr713tPbjzPhCCbf"} track {uri:
//              "spotify:episode:59PP5P8bF17KnzueM9DUH5"} track {uri: "spotify:episode:4nxFKKH8UPXrYUhbD2G0Db"} track {uri:
//              "spotify:episode:4CU6IsMhkflwdQcXyXPxrs"} track {uri: "spotify:episode:0VfUuKQWnttO6XsffakAnF"} track {uri:
//              "spotify:episode:5ZNj0vDZJOYTHRpaCJExCl"} track {uri: "spotify:episode:3SV9ap9wMxpDKHaeEv0kuc"} track {uri:
//              "spotify:episode:7oe8eW41Vrh2xJAUOQRsvF"})xx"};


//     auto onStderrMsg = [](const std::string& line)
//     {
//         std::string LOG_TAG = "xxx";
//         smatch m;
//         static regex re_log_line(R"(\[(\S+)(\s+)(\bTRACE\b|\bDEBUG\b|\bINFO\b|\bWARN\b|\bERROR\b)(\s+)(.*)\] (.*))");
//         if (regex_search(line, m, re_log_line) && (m.size() == 7))
//         {
//             const std::string& level = m[3];
//             const std::string& source = m[5];
//             const std::string& message = m[6];
//             AixLog::Severity severity = AixLog::Severity::info;
//             if (level == "TRACE")
//                 severity = AixLog::Severity::trace;
//             else if (level == "DEBUG")
//                 severity = AixLog::Severity::debug;
//             else if (level == "INFO")
//                 severity = AixLog::Severity::info;
//             else if (level == "WARN")
//                 severity = AixLog::Severity::warning;
//             else if (level == "ERROR")
//                 severity = AixLog::Severity::error;

//             LOG(severity, source) << message << "\n";
//         }
//         else
//             LOG(INFO, LOG_TAG) << line << "\n";

//         // Librespot patch:
//         // 	info!("metadata:{{\"ARTIST\":\"{}\",\"TITLE\":\"{}\"}}", artist.name, track.name);
//         // non patched:
//         //  [2021-06-04T07:20:47Z INFO  librespot_playback::player] <Tunnel> (310573 ms) loaded
//         // 	info!("Track \"{}\" loaded", track.name);
//         // std::cerr << line << "\n";
//         static regex re_patched("metadata:(.*)");
//         static regex re_track_loaded(R"( <(.*)> \((.*) ms\) loaded)");
//         // Parse the patched version
//         if (regex_search(line, m, re_patched))
//         {
//             // Patched version
//             LOG(INFO, LOG_TAG) << "metadata: <" << m[1] << ">\n";
//             json j = json::parse(m[1].str());
//             Metadata meta;
//             meta.artist = std::vector<std::string>{j["ARTIST"].get<std::string>()};
//             meta.title = j["TITLE"].get<std::string>();
//             // meta.art_data = {SPOTIFY_LOGO, "svg"};
//             Properties properties;
//             properties.metadata = std::move(meta);
//             // setProperties(properties);
//         }
//         else if (regex_search(line, m, re_track_loaded))
//         {
//             LOG(INFO, LOG_TAG) << "metadata: <" << m[1] << ">\n";
//             Metadata meta;
//             meta.title = string(m[1]);
//             meta.duration = cpt::stod(m[2]) / 1000.;
//             // meta.art_data = {SPOTIFY_LOGO, "svg"};
//             Properties properties;
//             properties.metadata = std::move(meta);
//             // setProperties(properties);
//         }
//     };

//     for (const auto& line : loglines)
//     {
//         onStderrMsg(line);
//     }
//     REQUIRE(true);
// }


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
