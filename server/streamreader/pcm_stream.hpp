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

#ifndef PCM_STREAM_HPP
#define PCM_STREAM_HPP

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-braces"
#include <boost/process.hpp>
#pragma GCC diagnostic pop
#include <atomic>
#include <condition_variable>
#include <map>
#include <string>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/optional.hpp>

#include "common/json.hpp"
#include "common/metatags.hpp"
#include "common/properties.hpp"
#include "common/sample_format.hpp"
#include "encoder/encoder.hpp"
#include "message/codec_header.hpp"
// #include "message/stream_tags.hpp"
#include "jsonrpcpp.hpp"
#include "server_settings.hpp"
#include "stream_uri.hpp"


namespace bp = boost::process;
using json = nlohmann::json;


namespace streamreader
{

class PcmStream;

enum class ReaderState
{
    kUnknown = 0,
    kIdle = 1,
    kPlaying = 2,
    kDisabled = 3
};


static std::string to_string(const ReaderState& reader_state)
{
    switch (reader_state)
    {
        case ReaderState::kIdle:
            return "idle";
        case ReaderState::kPlaying:
            return "playing";
        case ReaderState::kDisabled:
            return "disabled";
        case ReaderState::kUnknown:
        default:
            return "unknown";
    }
}


static std::ostream& operator<<(std::ostream& os, const ReaderState& reader_state)
{
    os << to_string(reader_state);
    return os;
}



static constexpr auto kUriCodec = "codec";
static constexpr auto kUriName = "name";
static constexpr auto kUriSampleFormat = "sampleformat";
static constexpr auto kUriChunkMs = "chunk_ms";
static constexpr auto kControlScript = "controlscript";


/// Callback interface for users of PcmStream
/**
 * Users of PcmStream should implement this to get the data
 */
class PcmListener
{
public:
    virtual void onMetaChanged(const PcmStream* pcmStream) = 0;
    virtual void onPropertiesChanged(const PcmStream* pcmStream) = 0;
    virtual void onStateChanged(const PcmStream* pcmStream, ReaderState state) = 0;
    virtual void onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk) = 0;
    virtual void onChunkEncoded(const PcmStream* pcmStream, std::shared_ptr<msg::PcmChunk> chunk, double duration) = 0;
    virtual void onResync(const PcmStream* pcmStream, double ms) = 0;
};


class CtrlScript
{
public:
    using OnRequest = std::function<void(const jsonrpcpp::Request& response)>;
    using OnNotification = std::function<void(const jsonrpcpp::Notification& response)>;
    using OnResponse = std::function<void(const jsonrpcpp::Response& response)>;
    using OnLog = std::function<void(std::string message)>;

    CtrlScript(boost::asio::io_context& ioc, const std::string& script);
    virtual ~CtrlScript();

    void start(const std::string& stream_id, const ServerSettings& server_setttings, const OnNotification& notification_handler,
               const OnRequest& request_handler, const OnLog& log_handler);
    void stop();
    /// Send a message to stdin of the process
    void send(const jsonrpcpp::Request& request, const OnResponse& response_handler);

private:
    void stderrReadLine();
    void stdoutReadLine();

    bp::child process_;
    bp::pipe pipe_stdout_;
    bp::pipe pipe_stderr_;
    std::unique_ptr<boost::asio::posix::stream_descriptor> stream_stdout_;
    std::unique_ptr<boost::asio::posix::stream_descriptor> stream_stderr_;
    boost::asio::streambuf streambuf_stdout_;
    boost::asio::streambuf streambuf_stderr_;
    OnRequest request_handler_;
    OnNotification notification_handler_;
    OnLog log_handler_;

    boost::asio::io_context& ioc_;
    std::string script_;
    bp::opstream in_;

    std::map<jsonrpcpp::Id, OnResponse> request_callbacks_;
};


/// Reads and decodes PCM data
/**
 * Reads PCM and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class PcmStream
{
public:
    /// ctor. Encoded PCM data is passed to the PcmListener
    PcmStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);
    virtual ~PcmStream();

    virtual void start();
    virtual void stop();

    virtual std::shared_ptr<msg::CodecHeader> getHeader();

    virtual const StreamUri& getUri() const;
    virtual const std::string& getName() const;
    virtual const std::string& getId() const;
    virtual const SampleFormat& getSampleFormat() const;
    virtual std::string getCodec() const;

    std::shared_ptr<Metatags> getMeta() const;
    std::shared_ptr<Properties> getProperties() const;

    virtual void setProperty(const jsonrpcpp::Request& request, const CtrlScript::OnResponse& response_handler);
    virtual void control(const jsonrpcpp::Request& request, const CtrlScript::OnResponse& response_handler);

    virtual ReaderState getState() const;
    virtual json toJson() const;

    void addListener(PcmListener* pcmListener);

protected:
    std::atomic<bool> active_;

    void onControlRequest(const jsonrpcpp::Request& request);
    void onControlNotification(const jsonrpcpp::Notification& notification);
    void onControlLog(std::string line);

    void setState(ReaderState newState);
    void chunkRead(const msg::PcmChunk& chunk);
    void resync(const std::chrono::nanoseconds& duration);
    void chunkEncoded(const encoder::Encoder& encoder, std::shared_ptr<msg::PcmChunk> chunk, double duration);

    void setMeta(const Metatags& meta);
    void setProperties(const Properties& props);

    std::chrono::time_point<std::chrono::steady_clock> tvEncodedChunk_;
    std::vector<PcmListener*> pcmListeners_;
    StreamUri uri_;
    SampleFormat sampleFormat_;
    size_t chunk_ms_;
    std::unique_ptr<encoder::Encoder> encoder_;
    std::string name_;
    ReaderState state_;
    std::shared_ptr<Metatags> meta_;
    std::shared_ptr<Properties> properties_;
    boost::asio::io_context& ioc_;
    ServerSettings server_settings_;
    std::unique_ptr<CtrlScript> ctrl_script_;
    size_t req_id_;
};

} // namespace streamreader

#endif
