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


// prototype/interface header file
#include "process_stream.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/utils.hpp"
#include "common/utils/file_utils.hpp"
#include "common/utils/string_utils.hpp"

// standard headers
#include <climits>
#include <cstdio>


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "ProcessStream";


ProcessStream::ProcessStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : AsioStream<stream_descriptor>(pcmListener, ioc, server_settings, uri)
{
    params_ = uri_.getQuery("params");
    wd_timeout_sec_ = cpt::stoul(uri_.getQuery("wd_timeout", "0"));
    LOG(DEBUG, LOG_TAG) << "Watchdog timeout: " << wd_timeout_sec_ << "\n";
    logStderr_ = (uri_.getQuery("log_stderr", "false") == "true");
}


std::string ProcessStream::findExe(const std::string& filename) const
{
    /// check if filename exists
    if (utils::file::exists(filename))
        return filename;

    std::string exe = filename;
    if (exe.find('/') != string::npos)
        exe = exe.substr(exe.find_last_of('/') + 1);

    /// check with "which"
    string which = execGetOutput("which " + exe);
    if (!which.empty())
        return which;

    /// check in the same path as this binary
    char buff[PATH_MAX];
    char szTmp[32];
    sprintf(szTmp, "/proc/%d/exe", getpid());
    ssize_t len = readlink(szTmp, buff, sizeof(buff) - 1);
    if (len != -1)
    {
        buff[len] = '\0';
        return string(buff) + "/" + exe;
    }

    return "";
}


void ProcessStream::initExeAndPath(const std::string& filename)
{
    path_ = "";
    exe_ = findExe(filename);
    if (exe_.find('/') != string::npos)
    {
        path_ = exe_.substr(0, exe_.find_last_of('/') + 1);
        exe_ = exe_.substr(exe_.find_last_of('/') + 1);
    }

    if (!utils::file::exists(path_ + exe_))
        throw SnapException("file not found: \"" + filename + "\"");
}


void ProcessStream::connect()
{
    if (!active_)
        return;
    initExeAndPath(uri_.path);
    LOG(DEBUG, LOG_TAG) << "Launching: '" << path_ + exe_ << "', with params: '" << params_ << "', in path: '" << path_ << "'\n";

    pipe_stdout_ = bp::pipe();
    // could use bp::async_pipe, but this is broken in boost 1.72:
    // https://github.com/boostorg/process/issues/116
    pipe_stderr_ = bp::pipe();
    // stdout pipe should not block
    int flags = fcntl(pipe_stdout_.native_source(), F_GETFL, 0);
    fcntl(pipe_stdout_.native_source(), F_SETFL, flags | O_NONBLOCK);

    process_ = bp::child(path_ + exe_ + " " + params_, bp::std_out > pipe_stdout_, bp::std_err > pipe_stderr_, bp::start_dir = path_);
    stream_ = make_unique<stream_descriptor>(strand_, pipe_stdout_.native_source());
    stream_stderr_ = make_unique<stream_descriptor>(strand_, pipe_stderr_.native_source());
    on_connect();
    if (wd_timeout_sec_ > 0)
    {
        watchdog_ = make_unique<Watchdog>(strand_, this);
        watchdog_->start(std::chrono::seconds(wd_timeout_sec_));
    }
    else
    {
        watchdog_ = nullptr;
    }
    stderrReadLine();
}


void ProcessStream::disconnect()
{
    if (process_.running())
        ::kill(-process_.native_handle(), SIGINT);
    AsioStream<stream_descriptor>::disconnect();
}


void ProcessStream::onStderrMsg(const std::string& line)
{
    if (logStderr_)
    {
        LOG(INFO, LOG_TAG) << "(" << getName() << ") " << line << "\n";
    }
}


void ProcessStream::stderrReadLine()
{
    const std::string delimiter = "\n";
    boost::asio::async_read_until(*stream_stderr_, streambuf_stderr_, delimiter,
                                  [this, delimiter](const std::error_code& ec, std::size_t bytes_transferred)
                                  {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error while reading from stderr: " << ec.message() << "\n";
            return;
        }

        if (watchdog_)
            watchdog_->trigger();

        // Extract up to the first delimiter.
        std::string line{buffers_begin(streambuf_stderr_.data()), buffers_begin(streambuf_stderr_.data()) + bytes_transferred - delimiter.length()};
        if (!line.empty())
        {
            if (line.back() == '\r')
                line.resize(line.size() - 1);
            onStderrMsg(line);
        }
        streambuf_stderr_.consume(bytes_transferred);
        stderrReadLine();
    });
}


void ProcessStream::onTimeout(const Watchdog& /*watchdog*/, std::chrono::milliseconds ms)
{
    LOG(ERROR, LOG_TAG) << "Watchdog timeout: " << ms.count() / 1000 << "s\n";
    if (process_)
        ::kill(-process_.native_handle(), SIGINT);
}

} // namespace streamreader
