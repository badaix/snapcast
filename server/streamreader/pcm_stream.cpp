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
#include "pcm_stream.hpp"

// local headers
#include "base64.h"
#include "common/aixlog.hpp"
#include "common/error_code.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "common/utils/string_utils.hpp"
#include "control_error.hpp"
#include "encoder/encoder_factory.hpp"

// 3rd party headers
#include <boost/asio/ip/host_name.hpp>

// standard headers
#include <memory>


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "PcmStream";


PcmStream::PcmStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri)
    : active_(false), strand_(boost::asio::make_strand(ioc.get_executor())), pcmListeners_{pcmListener}, uri_(uri), chunk_ms_(20), state_(ReaderState::kIdle),
      server_settings_(server_settings), req_id_(0), property_timer_(strand_)
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
    chunk_ = std::make_unique<msg::PcmChunk>(sampleFormat_, chunk_ms_);
    silent_chunk_ = std::vector<char>(chunk_->payloadSize, 0);
    LOG(DEBUG, LOG_TAG) << "Chunk duration: " << chunk_->durationMs() << " ms, frames: " << chunk_->getFrameCount() << ", size: " << chunk_->payloadSize
                        << "\n";
    LOG(INFO, LOG_TAG) << "PcmStream: " << name_ << ", sampleFormat: " << sampleFormat_.toString() << "\n";

    if (uri_.query.find(kControlScript) != uri_.query.end())
    {
        std::string params;
        if (uri_.query.find(kControlScriptParams) != uri_.query.end())
            params = uri_.query[kControlScriptParams];
        stream_ctrl_ = std::make_unique<ScriptStreamControl>(strand_, uri_.query[kControlScript], params);
    }

    if (uri_.query.find(kUriChunkMs) != uri_.query.end())
        chunk_ms_ = cpt::stoul(uri_.query[kUriChunkMs]);

    double silence_threshold_percent = 0.;
    try
    {
        silence_threshold_percent = cpt::stod(uri_.getQuery("silence_threshold_percent", "0"));
    }
    catch (...)
    {
    }
    int32_t max_amplitude = std::pow(2, sampleFormat_.bits() - 1) - 1;
    silence_threshold_ = max_amplitude * (silence_threshold_percent / 100.);
    LOG(DEBUG, LOG_TAG) << "Silence threshold percent: " << silence_threshold_percent << ", silence threshold amplitude: " << silence_threshold_ << "\n";
}


PcmStream::~PcmStream()
{
    stop();
    property_timer_.cancel();
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


void PcmStream::pollProperties()
{
    property_timer_.expires_after(10s);
    property_timer_.async_wait(
        [this](const boost::system::error_code& ec)
        {
        if (!ec)
        {
            stream_ctrl_->command({++req_id_, "Plugin.Stream.Player.GetProperties"},
                                  [this](const jsonrpcpp::Response& response)
                                  {
                LOG(INFO, LOG_TAG) << "Response for Plugin.Stream.Player.GetProperties: " << response.to_json() << "\n";
                if (response.error().code() == 0)
                    setProperties(response.result());
            });
            pollProperties();
        }
    });
}


void PcmStream::onControlNotification(const jsonrpcpp::Notification& notification)
{
    try
    {
        LOG(DEBUG, LOG_TAG) << "Notification method: " << notification.method() << ", params: " << notification.params().to_json() << "\n";
        if (notification.method() == "Plugin.Stream.Player.Properties")
        {
            LOG(DEBUG, LOG_TAG) << "Received properties notification\n";
            setProperties(notification.params().to_json());
        }
        else if (notification.method() == "Plugin.Stream.Ready")
        {
            LOG(DEBUG, LOG_TAG) << "Plugin is ready\n";
            stream_ctrl_->command({++req_id_, "Plugin.Stream.Player.GetProperties"},
                                  [this](const jsonrpcpp::Response& response)
                                  {
                LOG(INFO, LOG_TAG) << "Response for Plugin.Stream.Player.GetProperties: " << response.to_json() << "\n";
                if (response.error().code() == 0)
                    setProperties(response.result());
            });

            // TODO: Add capabilities or settings?
            // {"jsonrpc": "2.0", "method": "Plugin.Stream.Ready", "params": {"pollProperties": 10, "responseTimeout": 5}}
            // pollProperties();
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

    if (stream_ctrl_)
    {
        stream_ctrl_->start(
            getId(), server_settings_, [this](const jsonrpcpp::Notification& notification) { onControlNotification(notification); },
            [this](const jsonrpcpp::Request& request) { onControlRequest(request); }, [this](std::string message) { onControlLog(std::move(message)); });
    }

    active_ = true;
}


void PcmStream::stop()
{
    active_ = false;
    setState(ReaderState::kIdle);
}


bool PcmStream::isSilent(const msg::PcmChunk& chunk) const
{
    if (silence_threshold_ == 0)
        return (std::memcmp(chunk.payload, silent_chunk_.data(), silent_chunk_.size()) == 0);

    if (sampleFormat_.sampleSize() == 1)
    {
        auto payload = chunk.getPayload<int8_t>();
        for (size_t n = 0; n < payload.second; ++n)
        {
            if (abs(payload.first[n]) > silence_threshold_)
                return false;
        }
    }
    else if (sampleFormat_.sampleSize() == 2)
    {
        auto payload = chunk.getPayload<int16_t>();
        for (size_t n = 0; n < payload.second; ++n)
        {
            if (abs(payload.first[n]) > silence_threshold_)
                return false;
        }
    }
    else if (sampleFormat_.sampleSize() == 4)
    {
        auto payload = chunk.getPayload<int32_t>();
        for (size_t n = 0; n < payload.second; ++n)
        {
            if (abs(payload.first[n]) > silence_threshold_)
                return false;
        }
    }
    return true;
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
    // LOG(TRACE, LOG_TAG) << "onChunkEncoded: " << getName() << ", duration: " << duration
    //                     << " ms, compression ratio: " << 100 - ceil(100 * (chunk->durationMs() / duration)) << "%\n";
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    json j = {
        {"uri", uri_.toJson()},
        {"id", getId()},
        {"status", to_string(state_)},
    };

    j["properties"] = properties_.toJson();

    return j;
}


void PcmStream::addListener(PcmStream::Listener* pcmListener)
{
    pcmListeners_.push_back(pcmListener);
}


const Properties& PcmStream::getProperties() const
{
    // std::lock_guard<std::recursive_mutex> lock(mutex_);
    return properties_;
}


void PcmStream::setShuffle(bool shuffle, ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "setShuffle: " << shuffle << "\n";
    if (!properties_.can_control)
        return handler({ControlErrc::can_control_is_false});
    sendRequest("Plugin.Stream.Player.SetProperty", {"shuffle", shuffle}, std::move(handler));
}


void PcmStream::setLoopStatus(LoopStatus status, ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "setLoopStatus: " << status << "\n";
    if (!properties_.can_control)
        return handler({ControlErrc::can_control_is_false});
    sendRequest("Plugin.Stream.Player.SetProperty", {"loopStatus", to_string(status)}, std::move(handler));
}


void PcmStream::setVolume(uint16_t volume, ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "setVolume: " << volume << "\n";
    if (!properties_.can_control)
        return handler({ControlErrc::can_control_is_false});
    sendRequest("Plugin.Stream.Player.SetProperty", {"volume", volume}, std::move(handler));
}


void PcmStream::setMute(bool mute, ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "setMute: " << mute << "\n";
    if (!properties_.can_control)
        return handler({ControlErrc::can_control_is_false});
    sendRequest("Plugin.Stream.Player.SetProperty", {"mute", mute}, std::move(handler));
}


void PcmStream::setRate(float rate, ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "setRate: " << rate << "\n";
    if (!properties_.can_control)
        return handler({ControlErrc::can_control_is_false});
    sendRequest("Plugin.Stream.Player.SetProperty", {"rate", rate}, std::move(handler));
}


void PcmStream::setPosition(std::chrono::milliseconds position, ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "setPosition\n";
    if (!properties_.can_seek)
        return handler({ControlErrc::can_seek_is_false});
    json params;
    params["command"] = "setPosition";
    json j;
    j["position"] = position.count() / 1000.f;
    params["params"] = j;
    sendRequest("Plugin.Stream.Player.Control", params, std::move(handler));
}


void PcmStream::seek(std::chrono::milliseconds offset, ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "seek\n";
    if (!properties_.can_seek)
        return handler({ControlErrc::can_seek_is_false});
    json params;
    params["command"] = "seek";
    json j;
    j["offset"] = offset.count() / 1000.f;
    params["params"] = j;
    sendRequest("Plugin.Stream.Player.Control", params, std::move(handler));
}


void PcmStream::next(ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "next\n";
    if (!properties_.can_go_next)
        return handler({ControlErrc::can_go_next_is_false});
    sendRequest("Plugin.Stream.Player.Control", {"command", "next"}, std::move(handler));
}


void PcmStream::previous(ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "previous\n";
    if (!properties_.can_go_previous)
        return handler({ControlErrc::can_go_previous_is_false});
    sendRequest("Plugin.Stream.Player.Control", {"command", "previous"}, std::move(handler));
}


void PcmStream::pause(ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "pause\n";
    if (!properties_.can_pause)
        return handler({ControlErrc::can_pause_is_false});
    sendRequest("Plugin.Stream.Player.Control", {"command", "pause"}, std::move(handler));
}


void PcmStream::playPause(ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "playPause\n";
    if (!properties_.can_pause)
        return handler({ControlErrc::can_play_is_false});
    sendRequest("Plugin.Stream.Player.Control", {"command", "playPause"}, std::move(handler));
}


void PcmStream::stop(ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "stop\n";
    if (!properties_.can_control)
        return handler({ControlErrc::can_control_is_false});
    sendRequest("Plugin.Stream.Player.Control", {"command", "stop"}, std::move(handler));
}


void PcmStream::play(ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "play\n";
    if (!properties_.can_play)
        return handler({ControlErrc::can_play_is_false});
    sendRequest("Plugin.Stream.Player.Control", {"command", "play"}, std::move(handler));
}


void PcmStream::sendRequest(const std::string& method, const jsonrpcpp::Parameter& params, ResultHandler handler)
{
    if (!stream_ctrl_)
        return handler({ControlErrc::can_not_control});

    jsonrpcpp::Request req(++req_id_, method, params);
    stream_ctrl_->command(req,
                          [handler](const jsonrpcpp::Response& response)
                          {
        if (response.error().code() != 0)
            handler({static_cast<ControlErrc>(response.error().code()), response.error().data()});
        else
            handler({ControlErrc::success});
    });
}


void PcmStream::setProperties(const Properties& properties)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Properties props = properties;
    // Missing metadata means the data didn't change, so
    // enrich the new properites with old metadata
    if (!props.metadata.has_value() && properties_.metadata.has_value())
        props.metadata = properties_.metadata;

    // If the cover image is availbale as raw data, cache it on the HTTP Server to make it also available via HTTP
    if (props.metadata.has_value() && props.metadata->art_data.has_value() && !props.metadata->art_url.has_value())
    {
        auto data = base64_decode(props.metadata->art_data->data);
        auto md5 = ServerSettings::Http::image_cache.setImage(getName(), std::move(data), props.metadata->art_data->extension);

        std::stringstream url;
        url << "http://" << server_settings_.http.host << ":" << server_settings_.http.port << "/__image_cache?name=" << md5;
        props.metadata->art_url = url.str();
    }
    else if (!props.metadata.has_value() || !props.metadata->art_data.has_value())
    {
        ServerSettings::Http::image_cache.clear(getName());
    }

    if (props == properties_)
    {
        LOG(DEBUG, LOG_TAG) << "setProperties: Properties did not change\n";
        return;
    }

    properties_ = std::move(props);

    LOG(DEBUG, LOG_TAG) << "setProperties, stream: " << getId() << ", properties: " << properties_.toJson() << "\n";

    // Trigger a stream update
    for (auto* listener : pcmListeners_)
    {
        if (listener != nullptr)
            listener->onPropertiesChanged(this, properties_);
    }
}



} // namespace streamreader
