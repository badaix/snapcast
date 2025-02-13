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
#include "common/error_code.hpp"
#include "common/json.hpp"
#include "common/message/codec_header.hpp"
#include "common/sample_format.hpp"
#include "common/stream_uri.hpp"
#include "encoder/encoder.hpp"
#include "jsonrpcpp.hpp"
#include "properties.hpp"
#include "server_settings.hpp"
#include "stream_control.hpp"

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
class PcmStream : public std::enable_shared_from_this<PcmStream>
{
public:
    /// Callback interface for users of PcmStream
    /// Users of PcmStream should implement this to get the data
    class Listener
    {
    public:
        /// Properties of @p pcmStream changed to @p properties
        virtual void onPropertiesChanged(const PcmStream* pcmStream, const Properties& properties) = 0;
        /// State of @p pcmStream changed to @p state
        virtual void onStateChanged(const PcmStream* pcmStream, ReaderState state) = 0;
        /// Chunk @p chunk of @p pcmStream has read
        virtual void onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk) = 0;
        /// Chunk @p chunk with duration @p duration of stream @pcmStream has been encoded
        virtual void onChunkEncoded(const PcmStream* pcmStream, std::shared_ptr<msg::PcmChunk> chunk, double duration) = 0;
        /// Stream @p pcmStream muissed to read audio with duration @p ms
        virtual void onResync(const PcmStream* pcmStream, double ms) = 0;
    };


    using ResultHandler = std::function<void(const snapcast::ErrorCode& ec)>;

    /// c'tor. Encoded PCM data is passed to the PcmStream::Listener
    PcmStream(PcmStream::Listener* pcmListener, boost::asio::io_context& ioc, ServerSettings server_settings, StreamUri uri);
    /// d'tor
    virtual ~PcmStream();

    /// Start the stream reader, init the encoder and optionally the stream control
    virtual void start();
    /// Stop the stream reader
    virtual void stop();

    /// @return the codec header of the stream
    virtual std::shared_ptr<msg::CodecHeader> getHeader();

    /// @return the uri of the stream, as configured in snapserver.conf
    virtual const StreamUri& getUri() const;
    /// @return the name of the stream
    virtual const std::string& getName() const;
    /// @return the id of the stream
    virtual const std::string& getId() const;
    /// @return the sample format of the stream
    virtual const SampleFormat& getSampleFormat() const;
    /// @return the codec of the stream
    virtual std::string getCodec() const;

    /// @return stream properties
    const Properties& getProperties() const;

    // Setter for properties
    /// Set shuffle property
    virtual void setShuffle(bool shuffle, ResultHandler&& handler);
    /// Set loop property
    virtual void setLoopStatus(LoopStatus status, ResultHandler&& handler);
    /// Set volume property
    virtual void setVolume(uint16_t volume, ResultHandler&& handler);
    /// Set mute property
    virtual void setMute(bool mute, ResultHandler&& handler);
    /// Set playback rate property
    virtual void setRate(float rate, ResultHandler&& handler);

    // Control commands
    /// Set position
    virtual void setPosition(std::chrono::milliseconds position, ResultHandler&& handler);
    /// Seek
    virtual void seek(std::chrono::milliseconds offset, ResultHandler&& handler);
    /// Play next
    virtual void next(ResultHandler&& handler);
    /// Play previous
    virtual void previous(ResultHandler&& handler);
    /// Pause
    virtual void pause(ResultHandler&& handler);
    /// Toggle play/pause
    virtual void playPause(ResultHandler&& handler);
    /// Stop
    virtual void stop(ResultHandler&& handler);
    /// Play
    virtual void play(ResultHandler&& handler);

    /// Get stream reader state (idle/playing)
    virtual ReaderState getState() const;
    /// Stream description to json
    virtual json toJson() const;

    /// Add a pcm listener
    void addListener(PcmStream::Listener* pcmListener);

protected:
    /// Stream is active (started?
    std::atomic<bool> active_;

    /// check if the volume of the \p chunk is below the silence threshold
    bool isSilent(const msg::PcmChunk& chunk) const;

    /// Set reader state
    void setState(ReaderState newState);
    /// A @p chunk has been read
    void chunkRead(const msg::PcmChunk& chunk);
    /// Announce resync
    void resync(const std::chrono::nanoseconds& duration);
    /// Called by @p encoder when a @p chunk of @p duration ms has been encoded
    void chunkEncoded(const encoder::Encoder& encoder, const std::shared_ptr<msg::PcmChunk>& chunk, double duration);

    /// Set stream properties
    void setProperties(const Properties& properties);

    /// Poll stream properties
    void pollProperties();

    // script callbacks
    /// Request received from control script
    void onControlRequest(const jsonrpcpp::Request& request);
    /// Notification received from control script
    void onControlNotification(const jsonrpcpp::Notification& notification);
    /// Log message received from control script via stderr
    void onControlLog(std::string line);
    /// Send request to stream control script
    void sendRequest(const std::string& method, const jsonrpcpp::Parameter& params, ResultHandler&& handler);

    /// Executor for synchronous IO
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    /// Current abolute time of the last encoded chunk
    std::chrono::time_point<std::chrono::steady_clock> tvEncodedChunk_;
    /// Listeners for PCM events
    std::vector<PcmStream::Listener*> pcmListeners_;
    /// URI of this stream
    StreamUri uri_;
    /// Sampleformat of this stream
    SampleFormat sampleFormat_;
    /// Chunk read duration
    size_t chunk_ms_;
    /// Encoder (PCM, flac, vorbus, opus)
    std::unique_ptr<encoder::Encoder> encoder_;
    /// Name of this stream
    std::string name_;
    /// Stream state
    std::atomic<ReaderState> state_;
    /// Stream properies
    Properties properties_;
    /// Server settings
    ServerSettings server_settings_;
    /// Stream controller (play, pause, next, ...)
    std::unique_ptr<StreamControl> stream_ctrl_;
    /// Id of the last request sent to the stream controller
    std::atomic<int> req_id_;
    /// Property poll timer
    boost::asio::steady_timer property_timer_;
    /// Protect properties
    mutable std::recursive_mutex mutex_;
    /// If a chunk's max amplitude is below the threshold, it is considered silent
    int32_t silence_threshold_ = 0;
    /// Current chunk
    std::unique_ptr<msg::PcmChunk> chunk_;
    /// Silent chunk (all 0), for fast silence detection (memcmp)
    std::vector<char> silent_chunk_;
};

} // namespace streamreader
