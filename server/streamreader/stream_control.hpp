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
#include "jsonrpcpp.hpp"
#include "server_settings.hpp"

// 3rd party headers
#define BOOST_PROCESS_VERSION 1
#include <boost/asio.hpp>
#include <boost/process/v1/io.hpp>
#include <boost/process/v1/start_dir.hpp>
#include <boost/process/v1/system.hpp>

// standard headers
#include <filesystem>
#include <map>
#include <string>


namespace bp = boost::process;

using json = nlohmann::json;


namespace streamreader
{


/// Stream control base class
/// Controls a stream via "command" (play, pause, next, ...)
/// Provides status information (playback status, position, metadata, ...)
class StreamControl
{
public:
    /// Request handler coming from the plugin
    using OnRequest = std::function<void(const jsonrpcpp::Request& response)>;
    /// Notification handler coming from the plugin
    using OnNotification = std::function<void(const jsonrpcpp::Notification& response)>;
    /// Response handler coming from the plugin
    using OnResponse = std::function<void(const jsonrpcpp::Response& response)>;
    /// Log handler coming from the plugin
    using OnLog = std::function<void(std::string message)>;

    /// c'tor
    explicit StreamControl(const boost::asio::any_io_executor& executor);
    /// d'tor
    virtual ~StreamControl() = default;

    /// Start the stream control, calls abstract "doStart"
    void start(const std::string& stream_id, const ServerSettings& server_setttings, const OnNotification& notification_handler,
               const OnRequest& request_handler, const OnLog& log_handler);

    /// Issue a command to the stream, calls abstract "doCommand"
    void command(const jsonrpcpp::Request& request, const OnResponse& response_handler);

protected:
    /// abstract "command" interface: send a json request to the plugin
    virtual void doCommand(const jsonrpcpp::Request& request) = 0;
    /// abstract "start" interface: starts and initializes the plugin
    virtual void doStart(const std::string& stream_id, const ServerSettings& server_setttings) = 0;

    /// a @p json message has been received from the plugin
    void onReceive(const std::string& json);
    /// a @p message log request has been received from the plugin
    void onLog(std::string message);

    /// asio executor
    boost::asio::any_io_executor executor_;

private:
    OnRequest request_handler_;
    OnNotification notification_handler_;
    OnLog log_handler_;

    std::map<jsonrpcpp::Id, OnResponse> request_callbacks_;
};


/// Script based stream control
/// Executes a script (e.g. Python) and communicates via stdout/stdin with the script
class ScriptStreamControl : public StreamControl
{
public:
    /// c'tor
    ScriptStreamControl(const boost::asio::any_io_executor& executor, const std::filesystem::path& plugin_dir, std::string script, std::string params);
    /// d'tor
    virtual ~ScriptStreamControl() = default;

private:
    /// Send a message to stdin of the process
    void doCommand(const jsonrpcpp::Request& request) override;
    void doStart(const std::string& stream_id, const ServerSettings& server_setttings) override;

    void stderrReadLine();
    void stdoutReadLine();

    bp::child process_;
    bp::pipe pipe_stdout_;
    bp::pipe pipe_stderr_;
    std::unique_ptr<boost::asio::posix::stream_descriptor> stream_stdout_;
    std::unique_ptr<boost::asio::posix::stream_descriptor> stream_stderr_;
    boost::asio::streambuf streambuf_stdout_;
    boost::asio::streambuf streambuf_stderr_;

    std::string script_;
    std::string params_;
    bp::opstream in_;
};

} // namespace streamreader
