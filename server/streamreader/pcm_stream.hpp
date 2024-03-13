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

#ifndef PCM_STREAM_HPP
#define PCM_STREAM_HPP

// local headers
#include "common/error_code.hpp"
#include "common/json.hpp"
#include "common/message/codec_header.hpp"
#include "common/sample_format.hpp"
#include "encoder/encoder.hpp"
#include "jsonrpcpp.hpp"
#include "properties.hpp"
#include "server_settings.hpp"
#include "stream_control.hpp"
#include "stream_uri.hpp"

// 3rd party headers
#include <boost/asio/io_context.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>

// standard headers
#include <atomic>
#include <mutex>
#include <string>
#include <vector>



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
static constexpr auto kControlScriptParams = "controlscriptparams";


/// Reads and decodes PCM data
/**
 * Reads PCM and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmStream::Listener
 */
class PcmStream
{
public:
    /// Callback interface for users of PcmStream
    /**
     * Users of PcmStream should implement this to get the data
     */
    class Listener
    {
    public:
        virtual void onPropertiesChanged(const PcmStream* pcmStream, const Properties& properties) = 0;
        virtual void onStateChanged(const PcmStream* pcmStream, ReaderState state) = 0;
        virtual void onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk) = 0;
        virtual void onChunkEncoded(const PcmStream* pcmStream, std::shared_ptr<msg::PcmChunk> chunk, double duration) = 0;
        virtual void onResync(const PcmStream* pcmStream, double ms) = 0;
    };


    using ResultHandler = std::function<void(const snapcast::ErrorCode& ec)>;

    /// ctor. Encoded PCM data is passed to the PcmStream::Listener
    PcmStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, const ServerSettings& server_settings, const StreamUri& uri);
    virtual ~PcmStream();

    virtual void start();
    virtual void stop();

    virtual std::shared_ptr<msg::CodecHeader> getHeader();

    virtual const StreamUri& getUri() const;
    virtual const std::string& getName() const;
    virtual const std::string& getId() const;
    virtual const SampleFormat& getSampleFormat() const;
    virtual std::string getCodec() const;

    const Properties& getProperties() const;

    // Setter for properties
    virtual void setShuffle(bool shuffle, ResultHandler handler);
    virtual void setLoopStatus(LoopStatus status, ResultHandler handler);
    virtual void setVolume(uint16_t volume, ResultHandler handler);
    virtual void setMute(bool mute, ResultHandler handler);
    virtual void setRate(float rate, ResultHandler handler);

    // Control commands
    virtual void setPosition(std::chrono::milliseconds position, ResultHandler handler);
    virtual void seek(std::chrono::milliseconds offset, ResultHandler handler);
    virtual void next(ResultHandler handler);
    virtual void previous(ResultHandler handler);
    virtual void pause(ResultHandler handler);
    virtual void playPause(ResultHandler handler);
    virtual void stop(ResultHandler handler);
    virtual void play(ResultHandler handler);

    virtual ReaderState getState() const;
    virtual json toJson() const;

    void addListener(PcmStream::Listener* pcmListener);

protected:
    std::atomic<bool> active_;

    /// check if the volume of the \p chunk is below the silence threshold
    bool isSilent(const msg::PcmChunk& chunk) const;

    void setState(ReaderState newState);
    void chunkRead(const msg::PcmChunk& chunk);
    void resync(const std::chrono::nanoseconds& duration);
    void chunkEncoded(const encoder::Encoder& encoder, std::shared_ptr<msg::PcmChunk> chunk, double duration);

    void setProperties(const Properties& properties);

    void pollProperties();

    // script callbacks
    /// Request received from control script
    void onControlRequest(const jsonrpcpp::Request& request);
    /// Notification received from control script
    void onControlNotification(const jsonrpcpp::Notification& notification);
    /// Log message received from control script via stderr
    void onControlLog(std::string line);
    /// Send request to stream control script
    void sendRequest(const std::string& method, const jsonrpcpp::Parameter& params, ResultHandler handler);

    boost::asio::strand<boost::asio::any_io_executor> strand_;
    std::chrono::time_point<std::chrono::steady_clock> tvEncodedChunk_;
    std::vector<PcmStream::Listener*> pcmListeners_;
    StreamUri uri_;
    SampleFormat sampleFormat_;
    size_t chunk_ms_;
    std::unique_ptr<encoder::Encoder> encoder_;
    std::string name_;
    std::atomic<ReaderState> state_;
    Properties properties_;
    ServerSettings server_settings_;
    std::unique_ptr<StreamControl> stream_ctrl_;
    std::atomic<int> req_id_;
    boost::asio::steady_timer property_timer_;
    mutable std::recursive_mutex mutex_;
    /// If a chunk's max amplitude is below the threshold, it is considered silent
    int32_t silence_threshold_ = 0;
    /// Current chunk
    std::unique_ptr<msg::PcmChunk> chunk_;
    /// Silent chunk (all 0), for fast silence detection (memcmp)
    std::vector<char> silent_chunk_;
};


} // namespace streamreader

#endif
