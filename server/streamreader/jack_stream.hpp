/***
    This file is part of snapcast
    Copyright (C) 2014-2024  Johannes Pohl

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

#pragma once

#include <jack/jack.h>

// local headers
#include "pcm_stream.hpp"
#include <server/server_settings.hpp>

// 3rd party headers
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>


namespace streamreader
{


/// Reads and decodes PCM data from a Jack server
/**
 * Reads PCM from a Jack server and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmStream::Listener
 */
class JackStream : public PcmStream
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    JackStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);

    void start() override;
    void stop() override;

protected:
    bool openJackConnection();
    void closeJackConnection();
    bool createJackPorts();
    void tryConnect();
    int readJackBuffers(jack_nframes_t nframes);

    void onJackPortRegistration(jack_port_id_t port_id, int registered);
    void onJackShutdown();

    void autoConnectPorts();

    std::string serverName_;

    jack_client_t* client_;
    std::vector<jack_port_t*> ports_;
    jack_nframes_t jackConnectFrames_;
    std::chrono::time_point<std::chrono::steady_clock> jackConnectTime_;
    jack_time_t jackTimeAdjust_;
    SampleFormat jackSampleFormat_;

    std::string autoConnectRegex_;
    bool doAutoConnect_ = false;
    int autoConnectSkip_;

    void (*interleave_func_)(char*, jack_default_audio_sample_t*, unsigned long, unsigned long);

    boost::asio::steady_timer read_timer_;
    std::chrono::microseconds silence_;

    /// send silent chunks to clients
    bool send_silence_;
    /// silence duration before switching the stream to idle
    std::chrono::milliseconds idle_threshold_;

    static int processCallback(jack_nframes_t nframes, void* arg);
    static void jackShutdown(void* arg);
    static void jackErrorMessage(const char* msg);
    static void jackInfoMessage(const char* msg);
    static void jackPortConnect(jack_port_id_t a, jack_port_id_t b, int connect, void* arg);
    static void jackPortRegistration(jack_port_id_t port_id, int registered, void* arg);
};

} // namespace streamreader
