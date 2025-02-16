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

#pragma once


// local headers
#include "asio_stream.hpp"
#include "watchdog.hpp"

// standard headers
#include <memory>
#include <string>


namespace bp = boost::process;


namespace streamreader
{

using boost::asio::posix::stream_descriptor;

/// Starts an external process and reads and PCM data from stdout
/**
 * Starts an external process, reads PCM data from stdout, and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmStream::Listener
 */
class ProcessStream : public AsioStream<stream_descriptor>
{
public:
    /// c'tor. Encoded PCM data is passed to the PipeListener
    ProcessStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);
    /// d'tor
    ~ProcessStream() override = default;

protected:
    void connect() override;
    void disconnect() override;

    std::string exe_;      ///< filename of the process
    std::string path_;     ///< base path of the provess
    std::string params_;   ///< parameters for the process
    bp::pipe pipe_stdout_; ///< stdout of the process
    bp::pipe pipe_stderr_; ///< stderr of the process
    bp::child process_;    ///< the process

    bool logStderr_;                                   ///< log stderr to log?
    boost::asio::streambuf streambuf_stderr_;          ///< stderr read buffer
    std::unique_ptr<stream_descriptor> stream_stderr_; ///< stderr stream

    /// Read async from stderr
    virtual void stderrReadLine();
    /// Called for a line read from stderr
    virtual void onStderrMsg(const std::string& line);
    /// Try to find exe and base path, throw on error
    virtual void initExeAndPath(const std::string& filename);

    /// @return the executables complete path to @p filename
    std::string findExe(const std::string& filename) const;

    size_t wd_timeout_sec_;              ///< Watchdog timeout for arrival of new log lines
    std::unique_ptr<Watchdog> watchdog_; ///< the watchdog
    /// called on wd timeout, kills the process
    void onTimeout(std::chrono::milliseconds ms);
};

} // namespace streamreader
