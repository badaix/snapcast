/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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

// local headers
#include "common/aixlog.hpp"
#include "common/base64.h"
#include "common/error_code.hpp"
#include "common/jwt.hpp"
#include "common/utils/string_utils.hpp"
#include "server/authinfo.hpp"
#include "server/server_settings.hpp"
#include "server/streamreader/control_error.hpp"
#include "server/streamreader/properties.hpp"
#include "server/streamreader/stream_uri.hpp"

// 3rd party headers
#include <catch2/catch_test_macros.hpp>

// standard headers
#include <chrono>
#include <regex>
#include <system_error>
#include <vector>


using namespace std;


TEST_CASE("String utils")
{
    using namespace utils::string;
    REQUIRE(ltrim_copy(" test") == "test");

    auto strings = split("", '*');
    REQUIRE(strings.empty());

    strings = split("*", '*');
    REQUIRE(strings.size() == 2);
    REQUIRE(strings[0].empty());
    REQUIRE(strings[1].empty());

    strings = split("**", '*');
    REQUIRE(strings.size() == 3);
    REQUIRE(strings[0].empty());
    REQUIRE(strings[1].empty());
    REQUIRE(strings[2].empty());

    strings = split("1*2", '*');
    REQUIRE(strings.size() == 2);
    REQUIRE(strings[0] == "1");
    REQUIRE(strings[1] == "2");

    strings = split("1**2", '*');
    REQUIRE(strings.size() == 3);
    REQUIRE(strings[0] == "1");
    REQUIRE(strings[1].empty());
    REQUIRE(strings[2] == "2");

    strings = split("*1*2", '*');
    REQUIRE(strings.size() == 3);
    REQUIRE(strings[0].empty());
    REQUIRE(strings[1] == "1");
    REQUIRE(strings[2] == "2");

    strings = split("*1*2*", '*');
    REQUIRE(strings.size() == 4);
    REQUIRE(strings[0].empty());
    REQUIRE(strings[1] == "1");
    REQUIRE(strings[2] == "2");
    REQUIRE(strings[3].empty());

    std::vector<std::string> vec{"1", "2", "3"};
    REQUIRE(container_to_string(vec) == "1, 2, 3");
}


TEST_CASE("JWT")
{
#if 0
    /// ECDSA
    {
        const auto* key = "-----BEGIN EC PRIVATE KEY-----\n"
                          "MIHcAgEBBEIAarXYdFyTZIM2NGgbjPj8UBlBRYjDKrEfHIDuP1sYGLCJDqqx/GxH\n"
                          "DpP8mmpTL2dII8cZSbW7zqRv43ZcwK2dq3OgBwYFK4EEACOhgYkDgYYABADPIxdE\n"
                          "ubzaxsCtZAhbRnG3SJMZoD/0+sFO+r/5m9o4PPAz7cBnSEG3hBThsa+uBE+rfeah\n"
                          "RagSvgYzRyn85/N0YQBsE6V3htaOBrmoW4PBo/Lg0GwXe5qz7Fp98DuwpDC3OtXa\n"
                          "43Mgl0rbcwtQ6e0xcAxkLJ0tDfpomiM9Lj5gM+WT1g==\n"
                          "-----END EC PRIVATE KEY-----\n";

        const auto* cert = "-----BEGIN CERTIFICATE-----\n"
                           "MIICVjCCAbigAwIBAgIUBrGbikLGcul0Cia5tBCsPDm7/P4wCgYIKoZIzj0EAwIw\n"
                           "PTELMAkGA1UEBhMCREUxDDAKBgNVBAgMA05SVzEPMA0GA1UEBwwGQWFjaGVuMQ8w\n"
                           "DQYDVQQKDAZCYWRBaXgwHhcNMjQwNjAzMTk0NzEzWhcNMjUwNjAzMTk0NzEzWjA9\n"
                           "MQswCQYDVQQGEwJERTEMMAoGA1UECAwDTlJXMQ8wDQYDVQQHDAZBYWNoZW4xDzAN\n"
                           "BgNVBAoMBkJhZEFpeDCBmzAQBgcqhkjOPQIBBgUrgQQAIwOBhgAEAM8jF0S5vNrG\n"
                           "wK1kCFtGcbdIkxmgP/T6wU76v/mb2jg88DPtwGdIQbeEFOGxr64ET6t95qFFqBK+\n"
                           "BjNHKfzn83RhAGwTpXeG1o4Guahbg8Gj8uDQbBd7mrPsWn3wO7CkMLc61drjcyCX\n"
                           "SttzC1Dp7TFwDGQsnS0N+miaIz0uPmAz5ZPWo1MwUTAdBgNVHQ4EFgQUy+1q8Nh5\n"
                           "qbrckn9hdMT7WXhha2gwHwYDVR0jBBgwFoAUy+1q8Nh5qbrckn9hdMT7WXhha2gw\n"
                           "DwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgOBiwAwgYcCQgCYarPzdeupTvEs\n"
                           "CC5OlWj9pzUJDY4R0kaEe94g+KwTsgyaY1OzhHKwmu4E2ggrU0BN/HTpRYrYhtFz\n"
                           "HnDpYIN5XAJBK9fgry78uAiN4RZHPfWBKJ/NMcCZO/dXLbxoFlh2+6FXD/TmABHW\n"
                           "XsA/SmQzK2kJOKkBu9jBwNPiFZUQrJJ5msE=\n"
                           "-----END CERTIFICATE-----\n";

        Jwt jwt;
        jwt.setIat(std::chrono::seconds(1516239022));
        jwt.setSub("Badaix");
        std::optional<std::string> token = jwt.getToken(key);
        REQUIRE(token.has_value());
        LOG(INFO, "TEST") << "Token: " << *token << "\n";
        REQUIRE(token.value() == "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.eyJpYXQiOjE1MTYyMzkwMjIsInN1YiI6IkJhZGFpeCJ9.MIGHAkEDxp53c--"
                                 "MwMQAiTR5pnXlCunmqruZ8U6owKIfm06zgcAVswmTAhXFMIF28X9Yu2_F0o2LjgSWfBP9NEKg6BXy-gJCAevKxaKwxv1bbPOf7kGOeBzESuq8h_"
                                 "pCJwGEA1WW9tflQgpsU52oBmIA-6fAHvl_EVPh4KIXoTQ71BRfacDMDzG2AA");

        REQUIRE(jwt.parse(token.value(), cert));
        REQUIRE(jwt.getSub().has_value());
        REQUIRE(jwt.getSub().value() == "Badaix");
        REQUIRE(jwt.getIat().has_value());
        REQUIRE(jwt.getIat().value() == std::chrono::seconds(1516239022));
        REQUIRE(!jwt.getExp().has_value());
    }
#endif

    /// RSA keys
    {
        AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::debug);
        const auto* key = "-----BEGIN PRIVATE KEY-----\n"
                          "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCwFxHHKvV6THj1\n"
                          "VjvZQJIW+OjIKF9MPfIN8wJTXn+4EkBJCoBy0NfKHG+FCb90YCPzvLrjL8jKcfhT\n"
                          "/jpaStrYYjABXA1sJS9P+9cwiV9RwTxTrfOiss8F7eZdyZI9UogrPmQa7YbevmwB\n"
                          "XeXIqKzdWQbtQsOJQgoL3vIR7qjUpVQrXPwAYQ6pc7AxS5RHFy8V2Y3sh/+i0aS9\n"
                          "bH/276OAKZNwVIfUIcSVVbBAPvrheR2ezUVoKSumUzek/2uq3cLb5YNF4XSe1Ikv\n"
                          "16lRSGTIQ2KflSYn3mJldFjEKL3sgADTmMhKes+TVveNr7eCvynyDCtHdiLanOKY\n"
                          "PyyOXFTdAgMBAAECggEAAjunU6UBZsBhrUzKEVZ5XnYKxP+xZlmyat0Iy8QFmcY4\n"
                          "z076iNo0o6u/efVWb/RIfcQILlmHXJKG7BEWg4Qc/oRPkwjW47xHULvtw0AOt85G\n"
                          "mfy5UPgfBMvQRs1c/87piqYt0KNFTqhQCCa9GKcTmsf7p5ZtPTLw8Sxt2e6H8LsK\n"
                          "60Jzc2Yw2t3unEb1NnjsTgshjPwFcdrppyRAa2B0Wk3f4ADA1i4vDmTt2+jTq/Hp\n"
                          "yFWup58Djs+lyn4RLnp7jFD2KS/q+qVQsTfGcPeXLMIWHHQDwfjfkzdA74zNi+Pn\n"
                          "C4e/iXIsC7VH4BJ8qrVH20WqTXRyuZ1uEF+32XbcSQKBgQDAtBXbnuGhu2llnfP2\n"
                          "dxJ5qtRjxTHedZs9UMEuy5pkLMi4JsIdjf072lqizpG7DE9kfiERwXJ/Spe4OMMh\n"
                          "MvWBpnJieTHnouAMpDVootVbSCpikOSGzClHZwpl6KU7pv9Q+Hv2xkSnTJLsakSt\n"
                          "qlOsG6cwK56kXiwYG0RsAn6lhQKBgQDp7gNE4J6wf9SZHErpyh+65VqqcF5+chNq\n"
                          "DTwFlb7U31WcDOA3hUHaQfrlwfblMAgnlVMofopdP4KIIgSWE31cCziBp8ZRy/25\n"
                          "2/aNkDoPEN59Ibk1RWLYsCzcQIAQrTjvfDMn3An1E9B3qYFzfdLKZItb8p3cxpO/\n"
                          "sMUwQRqFeQKBgDb7av0lwQUPXwwiXDhnUvsp9b2dxxPNBIUjJGuApkWMzZxVWq9q\n"
                          "EuXf8FphjA0Nfx2SK0dQpaWSF+X1NB+l1YyvfBWCtO19eGXC+IYpZ6zK02UaKEoZ\n"
                          "uHFqAfp/vZ1ekZx9uYj4myAM5iLUU1Iltgf2P+arm3EUeYpLRWN39sCtAoGBALZ0\n"
                          "egBC4gLv8TXqp1Np3w26zdiaBFnDR/kzkVkZztnhx7gLIuaq/Q3q4HJLsvJXYETf\n"
                          "ZxjyeaD5ZCohvkn/sYsVBWG7JieuX5uTQN5xW5dcpOwcXYR7Nfmkj5jKhhh7wyin\n"
                          "So8QRIPujG6IuvsFbF+HxFpXBWGpUJv2mBZm8PShAoGBALU/KNl1El0HpxTr3wgj\n"
                          "YY6eU3snn0Oa5Ci3k/FMWd1QTtVsf4Mym0demxshdC75L+qzCS0m5jAo9tm0keY9\n"
                          "A4F3VRlsuUyuAja+qDU2xMo3jnFKOIyMfN4mVSiFkqnq3eQ4xHgViyIEyr+8AbA4\n"
                          "ajjiCZsv+OITxQ+TTHeGDsdD\n"
                          "-----END PRIVATE KEY-----\n";

        const auto* cert = "-----BEGIN CERTIFICATE-----\n"
                           "MIIE3TCCAsWgAwIBAgIUW+CDwjHiUAMf5yeFct8FatvUWhYwDQYJKoZIhvcNAQEL\n"
                           "BQAwVDELMAkGA1UEBhMCREUxDDAKBgNVBAgMA05SVzEPMA0GA1UEBwwGQWFjaGVu\n"
                           "MQ8wDQYDVQQKDAZCYWRhaXgxFTATBgNVBAMMDGxhcHRvcC5sb2NhbDAeFw0yNDA1\n"
                           "MTAxNTA2NDdaFw0yNTA5MjIxNTA2NDdaMFQxCzAJBgNVBAYTAkRFMQwwCgYDVQQI\n"
                           "DANOUlcxDzANBgNVBAcMBkFhY2hlbjEPMA0GA1UECgwGQmFkYWl4MRUwEwYDVQQD\n"
                           "DAxsYXB0b3AubG9jYWwwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCw\n"
                           "FxHHKvV6THj1VjvZQJIW+OjIKF9MPfIN8wJTXn+4EkBJCoBy0NfKHG+FCb90YCPz\n"
                           "vLrjL8jKcfhT/jpaStrYYjABXA1sJS9P+9cwiV9RwTxTrfOiss8F7eZdyZI9Uogr\n"
                           "PmQa7YbevmwBXeXIqKzdWQbtQsOJQgoL3vIR7qjUpVQrXPwAYQ6pc7AxS5RHFy8V\n"
                           "2Y3sh/+i0aS9bH/276OAKZNwVIfUIcSVVbBAPvrheR2ezUVoKSumUzek/2uq3cLb\n"
                           "5YNF4XSe1Ikv16lRSGTIQ2KflSYn3mJldFjEKL3sgADTmMhKes+TVveNr7eCvyny\n"
                           "DCtHdiLanOKYPyyOXFTdAgMBAAGjgaYwgaMwHwYDVR0jBBgwFoAUG790F3RT+ckl\n"
                           "UB16DIH7Hs5D0fYwCQYDVR0TBAIwADALBgNVHQ8EBAMCBPAwSQYDVR0RBEIwQIIM\n"
                           "bGFwdG9wLmxvY2FsggwxOTIuMTY4LjAuMzmCCTEyNy4wLjAuMYIKcnBpNS5sb2Nh\n"
                           "bIILMTkyLjE2OC4wLjMwHQYDVR0OBBYEFNxTG9vR5KC+XgGVsRb76DA7EKbiMA0G\n"
                           "CSqGSIb3DQEBCwUAA4ICAQA9cTmiDYiQKz/0BUMYA3xfy48ajS5F3PVOGRQsoOyO\n"
                           "gRcut5mxH9xzhJc2rbwRKyuZBZV8EuEDImxNf41L72nUUyrfOfC2AV92IeXMGVzg\n"
                           "Qvw8ypXTI3wRFFeWmjyuFK42wcVXlZd8Rx6hWz7BVDR9fBVSHV3I2Iyv98RVukwv\n"
                           "X2hnZu+6Bn8fJ890YMmpX1sAfgcSFY863zNyj3n3/tjoMYHSYwWMC5cEa6BotSjH\n"
                           "dAkjoIQwWwUAJWxzwOJwSolj81oM/7BcZ8UmHhGWuVYmxr/NfSfe7pFb0H6Fkktb\n"
                           "S+8qUjhIm8YsrC2qO6DCCiOWqSwxoyjmX1tap0IyciSfRyyFlgSBs4v8/Y33wIlW\n"
                           "ktH0A4i9CZPlOGTy8hDGhwAxGY8unfuDSoAUr4RQpoJDGSIEeyUpWbfUVxdXhSI9\n"
                           "FgkhWQq28cauJPRcbNILCmz+iUh2DlehlBW4lc6PwcBF5PRknAT+81lRRWPnw1nV\n"
                           "Qmg7wFnm+G1K6Ip2yz69hvywQddkZ3nTFcIxDfXWpD+8w+RdITI0Q9jNeKAuHRIq\n"
                           "eHg9/cG/dph/CE/W/mN1sjO5E+oj8wbguBmBe+r6Ga/nQUwSYAGsvr5JlmDFDwqR\n"
                           "Q3Q4ZajYb45p1McbRnN1ImovW9o6GOS3JUZcMBBLyZH0fmbikUb80bj++artyfAD\n"
                           "Iw==\n"
                           "-----END CERTIFICATE-----\n";

        Jwt jwt;
        jwt.setIat(std::chrono::system_clock::from_time_t(1516239022));
        jwt.setSub("Badaix");
        std::optional<std::string> token = jwt.getToken(key);
        REQUIRE(token.has_value());
        REQUIRE(token.value() ==
                "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.eyJpYXQiOjE1MTYyMzkwMjIsInN1YiI6IkJhZGFpeCJ9.LtKDGnT2OSgvWLECReajyMmUv7ApJeRu83MZhDM7d_"
                "1t1zy2Z08BQZEEB58WzR1vZAtRGHDVrYytJaVCPzibQN4eZ1F4m0gDk83hxQPTAKsbwjtzi7pUJzvaBa1ni4ysc9POtoi_M1OtNk5xxziyk5VP1Ph-"
                "TQbm9BCPfpA8bSUCx0LFrm5gyCD3irkww_W3RwDc2ghrjDCRNCyu4R9__lrCRGdx3Z8i0YMB_obuShcYzJXFSxG8adTSs3PQ_R4NXR94-vydVrvBxqe79apocFVrs_"
                "c9Ub8TIFynzqp9L_s206nb2N3C1WfUkKeQ1E7gAgVq8b4SM0OZsmkERQ0P0w");

        REQUIRE(jwt.parse(token.value(), cert));
        REQUIRE(jwt.getSub().has_value());
        REQUIRE(jwt.getSub().value() == "Badaix");
        REQUIRE(jwt.getIat().has_value());
        REQUIRE(jwt.getIat().value() == std::chrono::system_clock::from_time_t(1516239022));
        REQUIRE(!jwt.getExp().has_value());
    }
}


TEST_CASE("Uri")
{
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
    REQUIRE(uri.toString().find("spotify:///librespot?") == 0);
    StreamUri uri_from_str{uri.toString()};
    // REQUIRE(uri == uri_from_str);
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
    if (((title_pos = line.find('<')) != std::string::npos) && ((n = line.find('>', title_pos)) != std::string::npos) &&
        ((ms_pos = line.find('(', n)) != std::string::npos) && ((n = line.find("ms) loaded", ms_pos)) != std::string::npos))
    {
        title_pos += 1;
        std::string title = line.substr(title_pos, line.find('>', title_pos) - title_pos);
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
    std::cout << ec << '\n';

    ec = make_error_code(ControlErrc::can_not_control);
    REQUIRE(ec.category() == snapcast::error::control::category());
    std::cout << "Category: " << ec.category().name() << ", " << ec.message() << '\n';

    snapcast::ErrorCode error_code{};
    REQUIRE(!error_code);
}



TEST_CASE("ErrorOr")
{
    {
        snapcast::ErrorOr<std::string> error_or("test");
        REQUIRE(error_or.hasValue());
        REQUIRE(!error_or.hasError());
        // Get value by reference
        REQUIRE(error_or.getValue() == "test");
        // Move value out
        REQUIRE(error_or.takeValue() == "test");
        // Value has been moved out, get will return an empty string
        REQUIRE(error_or.getValue().empty());
    }

    {
        snapcast::ErrorOr<std::string> error_or(make_error_code(ControlErrc::can_not_control));
        REQUIRE(error_or.hasError());
        REQUIRE(!error_or.hasValue());
        // Get error by reference
        REQUIRE(error_or.getError() == make_error_code(ControlErrc::can_not_control));
        // Get error by reference
        REQUIRE(error_or.getError() == ControlErrc::can_not_control);
        // Get error by reference
        REQUIRE(error_or.getError() != ControlErrc::parse_error);
        // Get error by reference
        REQUIRE(error_or.getError() == snapcast::ErrorCode(ControlErrc::can_not_control));
        // Move error out
        REQUIRE(error_or.takeError() == snapcast::ErrorCode(ControlErrc::can_not_control));
        // Error is moved out, will return something else
        // REQUIRE(error_or.getError() != snapcast::ErrorCode(ControlErrc::can_not_control));
    }
}


TEST_CASE("WildcardMatch")
{
    using namespace utils::string;
    REQUIRE(wildcardMatch("*", "Server.getToken"));
    REQUIRE(wildcardMatch("Server.*", "Server.getToken"));
    REQUIRE(wildcardMatch("Server.getToken", "Server.getToken"));
    REQUIRE(wildcardMatch("*.getToken", "Server.getToken"));
    REQUIRE(wildcardMatch("*.get*", "Server.getToken"));
    REQUIRE(wildcardMatch("**.get*", "Server.getToken"));
    REQUIRE(wildcardMatch("*.get**", "Server.getToken"));
    REQUIRE(wildcardMatch("*.ge**t*", "Server.getToken"));

    REQUIRE(!wildcardMatch("*.set*", "Server.getToken"));
    REQUIRE(!wildcardMatch(".*", "Server.getToken"));
    REQUIRE(!wildcardMatch("*.get", "Server.getToken"));
    REQUIRE(wildcardMatch("*erver*get*", "Server.getToken"));
    REQUIRE(!wildcardMatch("*get*erver*", "Server.getToken"));
}


TEST_CASE("Auth")
{
    {
        ServerSettings settings;
        ServerSettings::User user("badaix:*:secret");
        REQUIRE(user.permissions.size() == 1);
        REQUIRE(user.permissions[0] == "*");
        settings.users.push_back(user);

        AuthInfo auth(settings);
        auto ec = auth.authenticateBasic(base64_encode("badaix:secret"));
        REQUIRE(!ec);
        REQUIRE(auth.hasAuthInfo());
        REQUIRE(auth.hasPermission("stream"));
    }

    {
        ServerSettings settings;
        ServerSettings::User user("badaix::secret");
        REQUIRE(user.permissions.empty());
        settings.users.push_back(user);

        AuthInfo auth(settings);
        auto ec = auth.authenticateBasic(base64_encode("badaix:secret"));
        REQUIRE(!ec);
        REQUIRE(auth.hasAuthInfo());
        REQUIRE(!auth.hasPermission("stream"));
    }

    {
        ServerSettings settings;
        ServerSettings::User user("badaix:*:secret");
        settings.users.push_back(user);

        AuthInfo auth(settings);
        auto ec = auth.authenticateBasic(base64_encode("badaix:wrong_password"));
        REQUIRE(ec == AuthErrc::wrong_password);
        REQUIRE(!auth.hasAuthInfo());
        REQUIRE(!auth.hasPermission("stream"));

        ec = auth.authenticateBasic(base64_encode("unknown_user:secret"));
        REQUIRE(ec == AuthErrc::unknown_user);
        REQUIRE(!auth.hasAuthInfo());
        REQUIRE(!auth.hasPermission("stream"));
    }
}
