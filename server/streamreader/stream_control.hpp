/***
    This file is part of snapcast
    Copyright (C) 2014-2022  Johannes Pohl

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

#ifndef STREAM_CONTROL_HPP
#define STREAM_CONTROL_HPP

// local headers
#include "jsonrpcpp.hpp"
#include "server_settings.hpp"

// 3rd party headers
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <boost/process.hpp>
#pragma GCC diagnostic pop
#include <boost/asio/any_io_executor.hpp>

// standard headers
#include <map>
#include <string>


namespace bp = boost::process;

using json = nlohmann::json;


namespace streamreader
{


class StreamControl
{
public:
    using OnRequest = std::function<void(const jsonrpcpp::Request& response)>;
    using OnNotification = std::function<void(const jsonrpcpp::Notification& response)>;
    using OnResponse = std::function<void(const jsonrpcpp::Response& response)>;
    using OnLog = std::function<void(std::string message)>;

    StreamControl(const boost::asio::any_io_executor& executor);
    virtual ~StreamControl() = default;

    void start(const std::string& stream_id, const ServerSettings& server_setttings, const OnNotification& notification_handler,
               const OnRequest& request_handler, const OnLog& log_handler);

    void command(const jsonrpcpp::Request& request, const OnResponse& response_handler);

protected:
    virtual void doCommand(const jsonrpcpp::Request& request) = 0;
    virtual void doStart(const std::string& stream_id, const ServerSettings& server_setttings) = 0;

    void onReceive(const std::string& json);
    void onLog(std::string message);

    boost::asio::any_io_executor executor_;

private:
    OnRequest request_handler_;
    OnNotification notification_handler_;
    OnLog log_handler_;

    std::map<jsonrpcpp::Id, OnResponse> request_callbacks_;
};


class ScriptStreamControl : public StreamControl
{
public:
    ScriptStreamControl(const boost::asio::any_io_executor& executor, const std::string& script, const std::string& params);
    virtual ~ScriptStreamControl() = default;

protected:
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

#endif
