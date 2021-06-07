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

#include <fcntl.h>
#include <memory>
#include <sys/stat.h>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"
#include "encoder/encoder_factory.hpp"
#include "pcm_stream.hpp"


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "PcmStream";
static constexpr auto SCRIPT_LOG_TAG = "Script";


CtrlScript::CtrlScript(boost::asio::io_context& ioc, const std::string& script) : ioc_(ioc), script_(script)
{
}


CtrlScript::~CtrlScript()
{
    stop();
}


void CtrlScript::start(const std::string& stream_id, const ServerSettings& server_setttings, const OnNotification& notification_handler,
                       const OnRequest& request_handler, const OnLog& log_handler)
{
    notification_handler_ = notification_handler;
    request_handler_ = request_handler;
    log_handler_ = log_handler;
    pipe_stderr_ = bp::pipe();
    pipe_stdout_ = bp::pipe();
    stringstream params;
    params << " \"--stream=" + stream_id + "\"";
    if (server_setttings.http.enabled)
        params << " --snapcast-port=" << server_setttings.http.port;
    process_ = bp::child(
        script_ + params.str(), bp::std_out > pipe_stdout_, bp::std_err > pipe_stderr_, bp::std_in < in_,
        bp::on_exit =
            [](int exit, const std::error_code& ec_in) {
                auto severity = AixLog::Severity::debug;
                if (exit != 0)
                    severity = AixLog::Severity::error;
                LOG(severity, SCRIPT_LOG_TAG) << "Exit code: " << exit << ", message: " << ec_in.message() << "\n";
            },
        ioc_);
    stream_stdout_ = make_unique<boost::asio::posix::stream_descriptor>(ioc_, pipe_stdout_.native_source());
    stream_stderr_ = make_unique<boost::asio::posix::stream_descriptor>(ioc_, pipe_stderr_.native_source());
    stdoutReadLine();
    stderrReadLine();
}


void CtrlScript::send(const jsonrpcpp::Request& request, const OnResponse& response_handler)
{
    if (response_handler)
        request_callbacks_[request.id()] = response_handler;

    std::string msg = request.to_json().dump() + "\n";
    LOG(INFO, SCRIPT_LOG_TAG) << "Sending request: " << msg;
    in_.write(msg.data(), msg.size());
    in_.flush();
}


void CtrlScript::stderrReadLine()
{
    const std::string delimiter = "\n";
    boost::asio::async_read_until(*stream_stderr_, streambuf_stderr_, delimiter, [this, delimiter](const std::error_code& ec, std::size_t bytes_transferred) {
        if (ec)
        {
            LOG(ERROR, SCRIPT_LOG_TAG) << "Error while reading from stderr: " << ec.message() << "\n";
            return;
        }

        // Extract up to the first delimiter.
        std::string line{buffers_begin(streambuf_stderr_.data()), buffers_begin(streambuf_stderr_.data()) + bytes_transferred - delimiter.length()};
        log_handler_(std::move(line));
        streambuf_stderr_.consume(bytes_transferred);
        stderrReadLine();
    });
}


void CtrlScript::stdoutReadLine()
{
    const std::string delimiter = "\n";
    boost::asio::async_read_until(*stream_stdout_, streambuf_stdout_, delimiter, [this, delimiter](const std::error_code& ec, std::size_t bytes_transferred) {
        if (ec)
        {
            LOG(ERROR, SCRIPT_LOG_TAG) << "Error while reading from stdout: " << ec.message() << "\n";
            return;
        }

        // Extract up to the first delimiter.
        std::string line{buffers_begin(streambuf_stdout_.data()), buffers_begin(streambuf_stdout_.data()) + bytes_transferred - delimiter.length()};

        jsonrpcpp::entity_ptr entity(nullptr);
        try
        {
            entity = jsonrpcpp::Parser::do_parse(line);
            if (!entity)
            {
                LOG(ERROR, LOG_TAG) << "Failed to parse message\n";
            }
            if (entity->is_notification())
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
                LOG(WARNING, LOG_TAG) << "Not handling message: " << line << "\n";
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

        streambuf_stdout_.consume(bytes_transferred);
        stdoutReadLine();
    });
}



void CtrlScript::stop()
{
    if (process_.running())
    {
        ::kill(-process_.native_handle(), SIGINT);
    }
}


PcmStream::PcmStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : active_(false), pcmListeners_{pcmListener}, uri_(uri), chunk_ms_(20), state_(ReaderState::kIdle), ioc_(ioc), server_settings_(server_settings), req_id_(0)
{
    encoder::EncoderFactory encoderFactory;
    if (uri_.query.find(kUriCodec) == uri_.query.end())
        throw SnapException("Stream URI must have a codec");
    encoder_ = encoderFactory.createEncoder(uri_.query[kUriCodec]);

    if (uri_.query.find(kUriName) == uri_.query.end())
        throw SnapException("Stream URI must have a name");
    name_ = uri_.query[kUriName];

    if (uri_.query.find(kUriSampleFormat) == uri_.query.end())
        throw SnapException("Stream URI must have a sampleformat");
    sampleFormat_ = SampleFormat(uri_.query[kUriSampleFormat]);
    LOG(INFO, LOG_TAG) << "PcmStream: " << name_ << ", sampleFormat: " << sampleFormat_.toString() << "\n";

    if (uri_.query.find(kControlScript) != uri_.query.end())
    {
        ctrl_script_ = std::make_unique<CtrlScript>(ioc, uri_.query[kControlScript]);
    }

    if (uri_.query.find(kUriChunkMs) != uri_.query.end())
        chunk_ms_ = cpt::stoul(uri_.query[kUriChunkMs]);

    // setMeta(json());
}


PcmStream::~PcmStream()
{
    stop();
}


std::shared_ptr<msg::CodecHeader> PcmStream::getHeader()
{
    return encoder_->getHeader();
}


const StreamUri& PcmStream::getUri() const
{
    return uri_;
}


const std::string& PcmStream::getName() const
{
    return name_;
}


const std::string& PcmStream::getId() const
{
    return getName();
}


const SampleFormat& PcmStream::getSampleFormat() const
{
    return sampleFormat_;
}


std::string PcmStream::getCodec() const
{
    return encoder_->name();
}


void PcmStream::onControlRequest(const jsonrpcpp::Request& request)
{
    LOG(INFO, LOG_TAG) << "Request: " << request.method() << ", id: " << request.id() << ", params: " << request.params().to_json() << "\n";
}


void PcmStream::onControlNotification(const jsonrpcpp::Notification& notification)
{
    LOG(INFO, LOG_TAG) << "Notification method: " << notification.method() << ", params: " << notification.params().to_json() << "\n";
    if (notification.method() == "Player.Metadata")
    {
        LOG(DEBUG, LOG_TAG) << "Received metadata notification\n";
        setMeta(notification.params().to_json());
    }
    else if (notification.method() == "Player.Properties")
    {
        LOG(DEBUG, LOG_TAG) << "Received properties notification\n";
        setProperties(notification.params().to_json());
    }
    else
        LOG(WARNING, LOG_TAG) << "Received unknown notification method: '" << notification.method() << "'\n";
}


void PcmStream::onControlLog(std::string line)
{
    if (line.back() == '\r')
        line.resize(line.size() - 1);
    if (line.empty())
        return;
    auto tmp = utils::string::tolower_copy(line);
    AixLog::Severity severity = AixLog::Severity::info;
    if (tmp.find(" trace") != string::npos)
        severity = AixLog::Severity::trace;
    else if (tmp.find(" debug") != string::npos)
        severity = AixLog::Severity::debug;
    else if (tmp.find(" info") != string::npos)
        severity = AixLog::Severity::info;
    else if (tmp.find(" warning") != string::npos)
        severity = AixLog::Severity::warning;
    else if (tmp.find(" error") != string::npos)
        severity = AixLog::Severity::error;
    else if ((tmp.find(" fatal") != string::npos) || (tmp.find(" critical") != string::npos))
        severity = AixLog::Severity::fatal;
    LOG(severity, SCRIPT_LOG_TAG) << "Stream: " << getId() << ", message: " << line << "\n";
}


void PcmStream::start()
{
    LOG(DEBUG, LOG_TAG) << "Start: " << name_ << ", type: " << uri_.scheme << ", sampleformat: " << sampleFormat_.toString() << ", codec: " << getCodec()
                        << "\n";
    encoder_->init([this](const encoder::Encoder& encoder, std::shared_ptr<msg::PcmChunk> chunk, double duration) { chunkEncoded(encoder, chunk, duration); },
                   sampleFormat_);
    active_ = true;

    if (ctrl_script_)
        ctrl_script_->start(
            getId(), server_settings_, [this](const jsonrpcpp::Notification& notification) { onControlNotification(notification); },
            [this](const jsonrpcpp::Request& request) { onControlRequest(request); }, [this](std::string message) { onControlLog(std::move(message)); });
}


void PcmStream::stop()
{
    active_ = false;
}


ReaderState PcmStream::getState() const
{
    return state_;
}


void PcmStream::setState(ReaderState newState)
{
    if (newState != state_)
    {
        LOG(INFO, LOG_TAG) << "State changed: " << name_ << ", state: " << state_ << " => " << newState << "\n";
        state_ = newState;
        for (auto* listener : pcmListeners_)
        {
            if (listener != nullptr)
                listener->onStateChanged(this, newState);
        }
    }
}


void PcmStream::chunkEncoded(const encoder::Encoder& encoder, std::shared_ptr<msg::PcmChunk> chunk, double duration)
{
    std::ignore = encoder;
    // LOG(TRACE, LOG_TAG) << "onChunkEncoded: " << getName() << ", duration: " << duration << " ms, compression ratio: " << 100 - ceil(100 *
    // (chunk->durationMs() / duration)) << "%\n";
    if (duration <= 0)
        return;

    // absolute start timestamp is the tvEncodedChunk_
    auto microsecs = std::chrono::duration_cast<std::chrono::microseconds>(tvEncodedChunk_.time_since_epoch()).count();
    chunk->timestamp.sec = microsecs / 1000000;
    chunk->timestamp.usec = microsecs % 1000000;

    // update tvEncodedChunk_ to the next chunk start by adding the current chunk duration
    tvEncodedChunk_ += std::chrono::nanoseconds(static_cast<std::chrono::nanoseconds::rep>(duration * 1000000));
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onChunkEncoded(this, chunk, duration);
    }
}


void PcmStream::chunkRead(const msg::PcmChunk& chunk)
{
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onChunkRead(this, chunk);
    }
    encoder_->encode(chunk);
}


void PcmStream::resync(const std::chrono::nanoseconds& duration)
{
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onResync(this, duration.count() / 1000000.);
    }
}


json PcmStream::toJson() const
{
    json j = {
        {"uri", uri_.toJson()},
        {"id", getId()},
        {"status", to_string(state_)},
    };

    if (meta_)
        j["meta"] = meta_->toJson();
    if (properties_)
        j["properties"] = properties_->toJson();

    return j;
}


void PcmStream::addListener(PcmListener* pcmListener)
{
    pcmListeners_.push_back(pcmListener);
}


std::shared_ptr<Metatags> PcmStream::getMeta() const
{
    return meta_;
}


std::shared_ptr<Properties> PcmStream::getProperties() const
{
    return properties_;
}


void PcmStream::setProperty(const jsonrpcpp::Request& request, const CtrlScript::OnResponse& response_handler)
{
    auto name = request.params().get("property");
    auto value = request.params().get("value");
    LOG(INFO, LOG_TAG) << "Stream '" << getId() << "' set property: " << name << " = " << value << "\n";
    // TODO: check validity
    bool valid = true;
    if (name == "loopStatus")
    {
        auto val = value.get<std::string>();
        valid = ((val == "none") || (val == "track") || (val == "playlist"));
    }
    else if (name == "shuffle")
    {
        valid = value.is_boolean();
    }
    else if (name == "volume")
    {
        valid = value.is_number_integer();
    }
    else if (name == "rate")
    {
        valid = value.is_number_float();
    }
    else
    {
        valid = false;
    }

    if (!valid)
    {
        auto error = jsonrpcpp::InvalidParamsException(request);
        response_handler(error.to_json());
        return;
    }

    if (ctrl_script_)
    {
        jsonrpcpp::Request req(++req_id_, "Player.SetProperty", {name, value});
        ctrl_script_->send(req, response_handler);
    }
}


void PcmStream::control(const jsonrpcpp::Request& request, const CtrlScript::OnResponse& response_handler)
{

    std::string command = request.params().get("command");
    static std::set<std::string> supported_commands{"Next", "Previous", "Pause", "PlayPause", "Stop", "Play", "Seek", "SetPosition"};
    if ((supported_commands.find(command) == supported_commands.end()) ||
        ((command == "SetPosition") &&
         (!request.params().has("params") || !request.params().get("params").contains("Position") || !request.params().get("params").contains("TrackId"))) ||
        ((command == "Seek") && (!request.params().has("params") || !request.params().get("params").contains("Offset"))))
    {
        auto error = jsonrpcpp::InvalidParamsException(request);
        response_handler(error.to_json());
        return;
    }

    LOG(INFO, LOG_TAG) << "Stream '" << getId() << "' received command: '" << command << "', params: '" << request.params().to_json() << "'\n";
    if (ctrl_script_)
    {
        jsonrpcpp::Parameter params{"command", command};
        if (request.params().has("params"))
            params.add("params", request.params().get("params"));
        jsonrpcpp::Request req(++req_id_, "Player.Control", params);
        ctrl_script_->send(req, response_handler);
    }
}


void PcmStream::setMeta(const Metatags& meta)
{
    meta_ = std::make_shared<Metatags>(meta);
    LOG(INFO, LOG_TAG) << "setMeta, stream: " << getId() << ", metadata: " << meta_->toJson() << "\n";

    // Trigger a stream update
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onMetaChanged(this);
    }
}


void PcmStream::setProperties(const Properties& props)
{
    properties_ = std::make_shared<Properties>(props);
    LOG(INFO, LOG_TAG) << "setProperties, stream: " << getId() << ", properties: " << props.toJson() << "\n";

    // Trigger a stream update
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onPropertiesChanged(this);
    }
}



} // namespace streamreader
