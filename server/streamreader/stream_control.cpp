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

// prototype/interface header file
#include "stream_control.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/file_utils.hpp"
#include "common/utils/string_utils.hpp"
#include "encoder/encoder_factory.hpp"

// 3rd party headers
#include <boost/asio/read_until.hpp>

// standard headers
#include <memory>


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "Script";


StreamControl::StreamControl(const boost::asio::any_io_executor& executor) : executor_(executor)
{
}



void StreamControl::start(const std::string& stream_id, const ServerSettings& server_setttings, const OnNotification& notification_handler,
                          const OnRequest& request_handler, const OnLog& log_handler)
{
    notification_handler_ = notification_handler;
    request_handler_ = request_handler;
    log_handler_ = log_handler;

    doStart(stream_id, server_setttings);
}


void StreamControl::command(const jsonrpcpp::Request& request, const OnResponse& response_handler)
{
    // use strand to serialize commands sent from different threads
    boost::asio::post(executor_,
                      [this, request, response_handler]()
                      {
        if (response_handler)
            request_callbacks_[request.id()] = response_handler;

        doCommand(request);
    });
}



void StreamControl::onReceive(const std::string& json)
{
    jsonrpcpp::entity_ptr entity(nullptr);
    try
    {
        entity = jsonrpcpp::Parser::do_parse(json);
        if (!entity)
        {
            LOG(ERROR, LOG_TAG) << "Failed to parse message\n";
        }
        else if (entity->is_notification())
        {
            jsonrpcpp::notification_ptr notification = dynamic_pointer_cast<jsonrpcpp::Notification>(entity);
            notification_handler_(*notification);
        }
        else if (entity->is_request())
        {
            jsonrpcpp::request_ptr request = dynamic_pointer_cast<jsonrpcpp::Request>(entity);
            request_handler_(*request);
        }
        else if (entity->is_response())
        {
            jsonrpcpp::response_ptr response = dynamic_pointer_cast<jsonrpcpp::Response>(entity);
            LOG(INFO, LOG_TAG) << "Response: " << response->to_json() << ", id: " << response->id() << "\n";
            // TODO: call request_callbacks_ on timeout with error
            auto iter = request_callbacks_.find(response->id());
            if (iter != request_callbacks_.end())
            {
                iter->second(*response);
                request_callbacks_.erase(iter);
            }
            else
            {
                LOG(WARNING, LOG_TAG) << "No request found for response with id: " << response->id() << "\n";
            }
        }
        else
        {
            LOG(WARNING, LOG_TAG) << "Not handling message: " << json << "\n";
        }
    }
    catch (const jsonrpcpp::ParseErrorException& e)
    {
        LOG(ERROR, LOG_TAG) << "Failed to parse message: " << e.what() << "\n";
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Failed to parse message: " << e.what() << "\n";
    }
}


void StreamControl::onLog(std::string message)
{
    log_handler_(std::move(message));
}



ScriptStreamControl::ScriptStreamControl(const boost::asio::any_io_executor& executor, const std::string& script, const std::string& params)
    : StreamControl(executor), script_(script), params_(params)
{
    namespace fs = utils::file;
    if (!fs::exists(script_))
    {
        std::string plugin_path = "/usr/share/snapserver/plug-ins/";
        if (fs::exists(plugin_path + script_))
            script_ = plugin_path + script_;
        else
            throw SnapException("Control script not found: \"" + script_ + "\"");
    }
}



void ScriptStreamControl::doStart(const std::string& stream_id, const ServerSettings& server_setttings)
{
    pipe_stderr_ = bp::pipe();
    pipe_stdout_ = bp::pipe();
    stringstream params;
    params << " " << params_;
    params << " \"--stream=" + stream_id + "\"";
    if (server_setttings.http.enabled)
    {
        params << " --snapcast-port=" << server_setttings.http.port;
        params << " --snapcast-host=" << server_setttings.http.host;
    }
    LOG(DEBUG, LOG_TAG) << "Starting control script: '" << script_ << "', params: '" << params.str() << "'\n";
    try
    {
        process_ = bp::child(
            script_ + params.str(), bp::std_out > pipe_stdout_, bp::std_err > pipe_stderr_, bp::std_in < in_,
            bp::on_exit =
                [](int exit, const std::error_code& ec_in)
            {
            auto severity = AixLog::Severity::debug;
            if (exit != 0)
                severity = AixLog::Severity::error;
            LOG(severity, LOG_TAG) << "Exit code: " << exit << ", message: " << ec_in.message() << "\n";
            });
    }
    catch (const std::exception& e)
    {
        throw SnapException("Failed to start control script: '" + script_ + "', exception: " + e.what());
    }

    stream_stdout_ = make_unique<boost::asio::posix::stream_descriptor>(executor_, pipe_stdout_.native_source());
    stream_stderr_ = make_unique<boost::asio::posix::stream_descriptor>(executor_, pipe_stderr_.native_source());
    stdoutReadLine();
    stderrReadLine();
}


void ScriptStreamControl::doCommand(const jsonrpcpp::Request& request)
{
    std::string msg = request.to_json().dump() + "\n";
    LOG(INFO, LOG_TAG) << "Sending request: " << msg;
    in_.write(msg.data(), msg.size());
    in_.flush();
}


void ScriptStreamControl::stderrReadLine()
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

        // Extract up to the first delimiter.
        std::string line{buffers_begin(streambuf_stderr_.data()), buffers_begin(streambuf_stderr_.data()) + bytes_transferred - delimiter.length()};
        onLog(std::move(line));

        streambuf_stderr_.consume(bytes_transferred);
        stderrReadLine();
    });
}


void ScriptStreamControl::stdoutReadLine()
{
    const std::string delimiter = "\n";
    boost::asio::async_read_until(*stream_stdout_, streambuf_stdout_, delimiter,
                                  [this, delimiter](const std::error_code& ec, std::size_t bytes_transferred)
                                  {
        if (ec)
        {
            LOG(ERROR, LOG_TAG) << "Error while reading from stdout: " << ec.message() << "\n";
            return;
        }

        // Extract up to the first delimiter.
        std::string line{buffers_begin(streambuf_stdout_.data()), buffers_begin(streambuf_stdout_.data()) + bytes_transferred - delimiter.length()};
        onReceive(line);

        streambuf_stdout_.consume(bytes_transferred);
        stdoutReadLine();
    });
}


} // namespace streamreader
