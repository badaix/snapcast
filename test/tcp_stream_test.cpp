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

#include "server/streamreader/tcp_stream.hpp"

#include <algorithm>
#include <boost/asio/write.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
#include <netinet/tcp.h>
#endif

using namespace std::chrono_literals;

namespace
{
struct Listener : streamreader::PcmStream::Listener
{
    std::vector<std::vector<char>> chunks;
    void onPropertiesChanged(const streamreader::PcmStream*, const Properties&) override
    {
    }
    void onStateChanged(const streamreader::PcmStream*, streamreader::ReaderState) override
    {
    }
    void onChunkRead(const streamreader::PcmStream*, const msg::PcmChunk& chunk) override
    {
        chunks.emplace_back(chunk.payload, chunk.payload + chunk.payloadSize);
    }
    void onChunkEncoded(const streamreader::PcmStream*, std::shared_ptr<msg::PcmChunk>, double) override
    {
    }
    void onResync(const streamreader::PcmStream*, double) override
    {
    }
};

struct TestStream : streamreader::TcpStream
{
    using TcpStream::TcpStream;
    tcp::socket& socket()
    {
        return *stream_;
    }
    // The encoder retains its owner through its callback; clear it in test cleanup.
    void releaseEncoder()
    {
        encoder_.reset();
    }
};

struct Fixture
{
    boost::asio::io_context io;
    Listener listener;
    std::shared_ptr<TestStream> source;
    unsigned short port;

    Fixture(const std::string& keepalive = "0")
    {
        tcp::acceptor reservation(io, tcp::endpoint(tcp::v4(), 0));
        port = reservation.local_endpoint().port();
        reservation.close();
        StreamUri uri("tcp://127.0.0.1:" + std::to_string(port) + "?name=test&codec=pcm&sampleformat=44100:16:2&keepalive=" + keepalive);
        source = std::make_shared<TestStream>(&listener, io, ServerSettings{}, uri, streamreader::PcmStream::Source::config);
        source->start();
    }
    ~Fixture()
    {
        if (source)
        {
            source->stop();
            source->releaseEncoder();
        }
        pump(5ms);
    }
    void pump(std::chrono::milliseconds duration)
    {
        io.restart();
        io.run_for(duration);
    }
    tcp::socket connect()
    {
        tcp::socket socket(io);
        socket.connect(tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), port));
        pump(2ms);
        return socket;
    }
    void send(tcp::socket& socket, char value, size_t size = 3528)
    {
        std::vector<char> data(size, value);
        boost::asio::write(socket, boost::asio::buffer(data));
    }
};
} // namespace

TEST_CASE("TCP replacement cancels the old pacing timer", "[tcp]")
{
    Fixture f;
    auto old = f.connect();
    f.send(old, 1);
    f.pump(2ms); // The completed chunk schedules the next read in 20ms.
    REQUIRE(f.listener.chunks.size() == 1);
    auto current = f.connect();
    f.pump(40ms); // Let the old pacing timer expire while the new read is pending.
    f.send(current, 2, 3528 * 3);
    f.pump(5ms);
    REQUIRE(f.listener.chunks.size() == 2); // Only one chunk before the next pacing tick.
    f.pump(80ms);
    REQUIRE(f.listener.chunks.size() == 4);
    for (size_t i = 1; i < f.listener.chunks.size(); ++i)
        REQUIRE(std::all_of(f.listener.chunks[i].begin(), f.listener.chunks[i].end(), [](char value) { return value == 2; }));
}

TEST_CASE("TCP replacement discards a partial chunk and survives EOF", "[tcp]")
{
    Fixture f;
    auto old = f.connect();
    f.send(old, 1, 100);
    f.pump(2ms);
    auto current = f.connect();
    f.send(current, 2);
    f.pump(30ms);
    REQUIRE(f.listener.chunks.size() == 1);
    REQUIRE(std::all_of(f.listener.chunks.back().begin(), f.listener.chunks.back().end(), [](char value) { return value == 2; }));
    current.close();
    auto next = f.connect();
    f.send(next, 3, 3528 * 2);
    f.pump(150ms); // Includes the old read-error reconnect delay.
    REQUIRE(f.listener.chunks.size() == 3);
    REQUIRE(std::all_of(f.listener.chunks.back().begin(), f.listener.chunks.back().end(), [](char value) { return value == 3; }));
}

TEST_CASE("Stopping TCP cancels acceptance and pending handlers", "[tcp]")
{
    Fixture f;
    auto old = f.connect();
    f.source->stop();
    auto late = f.connect();
    f.send(late, 4);
    f.pump(40ms);
    REQUIRE(f.listener.chunks.empty());
    f.pump(150ms);
    // Fixture and the encoder callback retain the source; no I/O handler should.
    REQUIRE(f.source.use_count() == 2);
}

TEST_CASE("TCP keepalive is configurable in both modes", "[tcp]")
{
    auto check = [](tcp::socket& socket, int idle)
    {
        boost::asio::socket_base::keep_alive enabled;
        socket.get_option(enabled);
        REQUIRE(enabled.value() == (idle > 0));
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
        if (idle > 0)
        {
            int value = 0;
            socklen_t size = sizeof(value);
#ifdef __APPLE__
            const int option = TCP_KEEPALIVE;
#else
            const int option = TCP_KEEPIDLE;
#endif
            REQUIRE(::getsockopt(socket.native_handle(), IPPROTO_TCP, option, &value, &size) == 0);
            REQUIRE(value == idle);
            REQUIRE(::getsockopt(socket.native_handle(), IPPROTO_TCP, TCP_KEEPINTVL, &value, &size) == 0);
            REQUIRE(value == 5);
            REQUIRE(::getsockopt(socket.native_handle(), IPPROTO_TCP, TCP_KEEPCNT, &value, &size) == 0);
            REQUIRE(value == 3);
        }
#endif
    };
    for (const auto& setting : {std::string("0"), std::string("7"), std::string("20")})
    {
        Fixture f(setting);
        auto sender = f.connect();
        check(f.source->socket(), std::stoi(setting));
        tcp::acceptor server(f.io, tcp::endpoint(tcp::v4(), 0));
        StreamUri uri("tcp://127.0.0.1:" + std::to_string(server.local_endpoint().port()) +
                      "?mode=client&name=client&codec=pcm&sampleformat=44100:16:2&keepalive=" + setting);
        auto client = std::make_shared<TestStream>(&f.listener, f.io, ServerSettings{}, uri, streamreader::PcmStream::Source::config);
        client->start();
        auto peer = server.accept();
        f.pump(5ms);
        check(client->socket(), std::stoi(setting));
        client->stop();
        client->releaseEncoder();
        f.pump(5ms);
    }
}
