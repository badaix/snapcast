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
        stream_ctrl_ = std::make_unique<ScriptStreamControl>(ioc, uri_.query[kControlScript]);
    }

    if (uri_.query.find(kUriChunkMs) != uri_.query.end())
        chunk_ms_ = cpt::stoul(uri_.query[kUriChunkMs]);
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
    try
    {
        LOG(INFO, LOG_TAG) << "Notification method: " << notification.method() << ", params: " << notification.params().to_json() << "\n";
        if (notification.method() == "Plugin.Stream.Player.Metadata")
        {
            LOG(DEBUG, LOG_TAG) << "Received metadata notification\n";
            setMetadata(notification.params().to_json());
        }
        else if (notification.method() == "Plugin.Stream.Player.Properties")
        {
            LOG(DEBUG, LOG_TAG) << "Received properties notification\n";
            setProperties(notification.params().to_json());
        }
        else if (notification.method() == "Plugin.Stream.Ready")
        {
            LOG(DEBUG, LOG_TAG) << "Plugin is ready\n";
            stream_ctrl_->command({++req_id_, "Plugin.Stream.Player.GetProperties"}, [this](const jsonrpcpp::Response& response) {
                LOG(INFO, LOG_TAG) << "Response for Plugin.Stream.Player.GetProperties: " << response.to_json() << "\n";
                if (response.error().code() == 0)
                    setProperties(response.result());
            });
            stream_ctrl_->command({++req_id_, "Plugin.Stream.Player.GetMetadata"}, [this](const jsonrpcpp::Response& response) {
                LOG(INFO, LOG_TAG) << "Response for Plugin.Stream.Player.GetMetadata: " << response.to_json() << "\n";
                if (response.error().code() == 0)
                    setMetadata(response.result());
            });
        }
        else if (notification.method() == "Plugin.Stream.Log")
        {
            std::string severity = notification.params().get("severity");
            std::string message = notification.params().get("message");
            LOG(INFO, LOG_TAG) << "Plugin log - severity: " << severity << ", message: " << message << "\n";
        }
        else
            LOG(WARNING, LOG_TAG) << "Received unknown notification method: '" << notification.method() << "'\n";
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "Error while receiving notification: " << e.what() << '\n';
    }
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
    LOG(severity, LOG_TAG) << "Stream: " << getId() << ", message: " << line << "\n";
}


void PcmStream::start()
{
    LOG(DEBUG, LOG_TAG) << "Start: " << name_ << ", type: " << uri_.scheme << ", sampleformat: " << sampleFormat_.toString() << ", codec: " << getCodec()
                        << "\n";
    encoder_->init([this](const encoder::Encoder& encoder, std::shared_ptr<msg::PcmChunk> chunk, double duration) { chunkEncoded(encoder, chunk, duration); },
                   sampleFormat_);
    active_ = true;

    if (stream_ctrl_)
    {
        stream_ctrl_->start(
            getId(), server_settings_, [this](const jsonrpcpp::Notification& notification) { onControlNotification(notification); },
            [this](const jsonrpcpp::Request& request) { onControlRequest(request); }, [this](std::string message) { onControlLog(std::move(message)); });
    }
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

    j["metadata"] = metadata_.toJson();
    j["properties"] = properties_.toJson();

    return j;
}


void PcmStream::addListener(PcmListener* pcmListener)
{
    pcmListeners_.push_back(pcmListener);
}


const Metatags& PcmStream::getMetadata() const
{
    return metadata_;
}


const Properties& PcmStream::getProperties() const
{
    return properties_;
}


void PcmStream::setProperty(const jsonrpcpp::Request& request, const StreamControl::OnResponse& response_handler)
{
    try
    {
        if (!request.params().has("property"))
            throw SnapException("Parameter 'property' is missing");

        if (!request.params().has("value"))
            throw SnapException("Parameter 'value' is missing");

        auto name = request.params().get("property");
        auto value = request.params().get("value");
        LOG(INFO, LOG_TAG) << "Stream '" << getId() << "' set property: " << name << " = " << value << "\n";

        if (name == "loopStatus")
        {
            auto val = value.get<std::string>();
            if ((val != "none") || (val != "track") || (val != "playlist"))
                throw SnapException("Value for loopStatus must be one of 'none', 'track', 'playlist'");
        }
        else if (name == "shuffle")
        {
            if (!value.is_boolean())
                throw SnapException("Value for shuffle must be bool");
        }
        else if (name == "volume")
        {
            if (!value.is_number_integer())
                throw SnapException("Value for volume must be an int");
        }
        else if (name == "rate")
        {
            if (!value.is_number_float())
                throw SnapException("Value for rate must be float");
        }

        if (!properties_.can_control)
            throw SnapException("CanControl is false");

        if (stream_ctrl_)
        {
            jsonrpcpp::Request req(++req_id_, "Plugin.Stream.Player.SetProperty", {name, value});
            stream_ctrl_->command(req, response_handler);
        }
    }
    catch (const std::exception& e)
    {
        LOG(WARNING, LOG_TAG) << "Error in setProperty: " << e.what() << '\n';
        auto error = jsonrpcpp::InvalidParamsException(e.what(), request.id());
        response_handler(error.to_json());
    }
}


void PcmStream::control(const jsonrpcpp::Request& request, const StreamControl::OnResponse& response_handler)
{
    try
    {
        if (!request.params().has("command"))
            throw SnapException("Parameter 'command' is missing");

        std::string command = request.params().get("command");
        if (command == "SetPosition")
        {
            if (!request.params().has("params") || !request.params().get("params").contains("Position") || !request.params().get("params").contains("TrackId"))
                throw SnapException("SetPosition requires parameters 'Position' and 'TrackId'");
            if (!properties_.can_seek)
                throw SnapException("CanSeek is false");
        }
        else if (command == "Seek")
        {
            if (!request.params().has("params") || !request.params().get("params").contains("Offset"))
                throw SnapException("Seek requires parameter 'Offset'");
            if (!properties_.can_seek)
                throw SnapException("CanSeek is false");
        }
        else if (command == "Next")
        {
            if (!properties_.can_go_next)
                throw SnapException("CanGoNext is false");
        }
        else if (command == "Previous")
        {
            if (!properties_.can_go_previous)
                throw SnapException("CanGoPrevious is false");
        }
        else if ((command == "Pause") || (command == "PlayPause"))
        {
            if (!properties_.can_pause)
                throw SnapException("CanPause is false");
        }
        else if (command == "Stop")
        {
            if (!properties_.can_control)
                throw SnapException("CanControl is false");
        }
        else if (command == "Play")
        {
            if (!properties_.can_play)
                throw SnapException("CanPlay is false");
        }
        else
            throw SnapException("Command not supported");

        LOG(INFO, LOG_TAG) << "Stream '" << getId() << "' received command: '" << command << "', params: '" << request.params().to_json() << "'\n";
        if (stream_ctrl_)
        {
            jsonrpcpp::Parameter params{"command", command};
            if (request.params().has("params"))
                params.add("params", request.params().get("params"));
            jsonrpcpp::Request req(++req_id_, "Plugin.Stream.Player.Control", params);
            stream_ctrl_->command(req, response_handler);
        }
    }
    catch (const std::exception& e)
    {
        LOG(WARNING, LOG_TAG) << "Error in control: " << e.what() << '\n';
        auto error = jsonrpcpp::InvalidParamsException(e.what(), request.id());
        response_handler(error.to_json());
    }
}


void PcmStream::setMetadata(const Metatags& metadata)
{
    if (metadata == metadata_)
    {
        LOG(DEBUG, LOG_TAG) << "setMetadata: Metadata did not change\n";
        return;
    }

    metadata_ = metadata;
    LOG(INFO, LOG_TAG) << "setMetadata, stream: " << getId() << ", metadata: " << metadata_.toJson() << "\n";

    // Trigger a stream update
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onMetadataChanged(this);
    }
}


void PcmStream::setProperties(const Properties& props)
{
    if (props == properties_)
    {
        LOG(DEBUG, LOG_TAG) << "setProperties: Properties did not change\n";
        return;
    }

    properties_ = props;
    LOG(INFO, LOG_TAG) << "setProperties, stream: " << getId() << ", properties: " << properties_.toJson() << "\n";

    // Trigger a stream update
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onPropertiesChanged(this);
    }
}



} // namespace streamreader
