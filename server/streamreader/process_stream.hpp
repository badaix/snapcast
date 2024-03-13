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

#ifndef PROCESS_STREAM_HPP
#define PROCESS_STREAM_HPP

// local headers
#include "asio_stream.hpp"
#include "watchdog.hpp"

// standard headers
#include <memory>
#include <string>
#include <vector>


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
class ProcessStream : public AsioStream<stream_descriptor>, public WatchdogListener
{
public:
    /// ctor. Encoded PCM data is passed to the PipeListener
    ProcessStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);
    ~ProcessStream() override = default;

protected:
    void connect() override;
    void disconnect() override;

    std::string exe_;
    std::string path_;
    std::string params_;
    bp::pipe pipe_stdout_;
    bp::pipe pipe_stderr_;
    bp::child process_;

    bool logStderr_;
    boost::asio::streambuf streambuf_stderr_;
    std::unique_ptr<stream_descriptor> stream_stderr_;

    // void worker() override;
    virtual void stderrReadLine();
    virtual void onStderrMsg(const std::string& line);
    virtual void initExeAndPath(const std::string& filename);

    std::string findExe(const std::string& filename) const;

    size_t wd_timeout_sec_;
    std::unique_ptr<Watchdog> watchdog_;
    void onTimeout(const Watchdog& watchdog, std::chrono::milliseconds ms) override;
};

} // namespace streamreader

#endif
