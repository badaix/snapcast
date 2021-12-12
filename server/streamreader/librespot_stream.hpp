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

#ifndef SPOTIFY_STREAM_HPP
#define SPOTIFY_STREAM_HPP

// local headers
#include "process_stream.hpp"

namespace streamreader
{

/// Starts librespot and reads PCM data from stdout
/**
 * Starts librespot, reads PCM data from stdout, and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmStream::Listener
 * usage:
 *   snapserver -s "spotify:///librespot?name=Spotify&username=<my username>&password=<my password>[&devicename=Snapcast][&bitrate=320][&volume=<volume in
 * percent>][&cache=<cache dir>]"
 */
class LibrespotStream : public ProcessStream
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    LibrespotStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);

protected:
    bool killall_;

    void onStderrMsg(const std::string& line) override;
    void initExeAndPath(const std::string& filename) override;
};

} // namespace streamreader

#endif
