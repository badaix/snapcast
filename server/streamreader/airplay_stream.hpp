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

#ifndef AIRPLAY_STREAM_HPP
#define AIRPLAY_STREAM_HPP

// local headers
#include "process_stream.hpp"

// 3rd party headers
// Expat is used in metadata parsing from Shairport-sync.
// Without HAS_EXPAT defined no parsing will occur.
#ifdef HAS_EXPAT
#include <expat.h>
#endif

namespace streamreader
{

class TageEntry
{
public:
    TageEntry() : isBase64(false), length(0)
    {
    }

    std::string code;
    std::string type;
    std::string data;
    bool isBase64;
    int length;
};

/// Starts shairport-sync and reads PCM data from stdout

/**
 * Starts librespot, reads PCM data from stdout, and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmStream::Listener
 * usage:
 *   snapserver -s "airplay:///shairport-sync?name=Airplay[&devicename=Snapcast][&port=5000]"
 */
class AirplayStream : public ProcessStream
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    AirplayStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);
    ~AirplayStream() override;

protected:
#ifdef HAS_EXPAT
    XML_Parser parser_;
    std::unique_ptr<TageEntry> entry_;
    std::string buf_;
    /// set whenever metadata_ has changed
    Metadata meta_;
    bool metadata_dirty_;
#endif

    void pipeReadLine();
#ifdef HAS_EXPAT
    int parse(const std::string& line);
    void createParser();
    void push();

    template <typename T>
    void setMetaData(std::optional<T>& meta_value, const T& value);
#endif

    void setParamsAndPipePathFromPort();

    void connect() override;
    void disconnect() override;
    void onStderrMsg(const std::string& line) override;
    void initExeAndPath(const std::string& filename) override;

    size_t port_;
    std::string pipePath_;
    std::string params_wo_port_;
    std::unique_ptr<boost::asio::posix::stream_descriptor> pipe_fd_;
    boost::asio::steady_timer pipe_open_timer_;
    boost::asio::streambuf streambuf_pipe_;

#ifdef HAS_EXPAT
    static void XMLCALL element_start(void* userdata, const char* element_name, const char** attr);
    static void XMLCALL element_end(void* userdata, const char* element_name);
    static void XMLCALL data(void* userdata, const char* content, int length);
#endif
};

} // namespace streamreader

#endif
