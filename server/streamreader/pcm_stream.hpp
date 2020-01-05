/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

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

#include "common/json.hpp"
#include "common/sample_format.hpp"
#include "encoder/encoder.hpp"
#include "message/codec_header.hpp"
#include "message/stream_tags.hpp"
#include "stream_uri.hpp"
#include <atomic>
#include <boost/asio/io_context.hpp>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>


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


static constexpr auto kUriCodec = "codec";
static constexpr auto kUriName = "name";
static constexpr auto kUriSampleFormat = "sampleformat";
static constexpr auto kUriChunkMs = "chunk_ms";


/// Callback interface for users of PcmStream
/**
 * Users of PcmStream should implement this to get the data
 */
class PcmListener
{
public:
    virtual void onMetaChanged(const PcmStream* pcmStream) = 0;
    virtual void onStateChanged(const PcmStream* pcmStream, const ReaderState& state) = 0;
    virtual void onChunkRead(const PcmStream* pcmStream, msg::PcmChunk* chunk, double duration) = 0;
    virtual void onResync(const PcmStream* pcmStream, double ms) = 0;
};


/// Reads and decodes PCM data
/**
 * Reads PCM and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class PcmStream : public encoder::EncoderListener
{
public:
    /// ctor. Encoded PCM data is passed to the PcmListener
    PcmStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri);
    virtual ~PcmStream();

    virtual void start();
    virtual void stop();

    /// Implementation of EncoderListener::onChunkEncoded
    void onChunkEncoded(const encoder::Encoder* encoder, msg::PcmChunk* chunk, double duration) override;
    virtual std::shared_ptr<msg::CodecHeader> getHeader();

    virtual const StreamUri& getUri() const;
    virtual const std::string& getName() const;
    virtual const std::string& getId() const;
    virtual const SampleFormat& getSampleFormat() const;

    std::shared_ptr<msg::StreamTags> getMeta() const;
    void setMeta(const json& j);

    virtual ReaderState getState() const;
    virtual json toJson() const;


protected:
    std::atomic<bool> active_;

    void setState(const ReaderState& newState);

    timeval tvEncodedChunk_;
    PcmListener* pcmListener_;
    StreamUri uri_;
    SampleFormat sampleFormat_;
    size_t chunk_ms_;
    std::unique_ptr<encoder::Encoder> encoder_;
    std::string name_;
    ReaderState state_;
    std::shared_ptr<msg::StreamTags> meta_;
    boost::asio::io_context& ioc_;
};

} // namespace streamreader

#endif
