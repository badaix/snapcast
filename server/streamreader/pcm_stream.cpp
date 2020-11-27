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

#include <fcntl.h>
#include <memory>
#include <sys/stat.h>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "encoder/encoder_factory.hpp"
#include "pcm_stream.hpp"


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "PcmStream";


PcmStream::PcmStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri)
    : active_(false), pcmListeners_{pcmListener}, uri_(uri), chunk_ms_(20), state_(ReaderState::kIdle), ioc_(ioc)
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

    if (uri_.query.find(kUriChunkMs) != uri_.query.end())
        chunk_ms_ = cpt::stoul(uri_.query[kUriChunkMs]);

    setMeta(json());
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


void PcmStream::start()
{
    LOG(DEBUG, LOG_TAG) << "Start: " << name_ << ", sampleformat: " << sampleFormat_.toString() << "\n";
    encoder_->init([this](const encoder::Encoder& encoder, std::shared_ptr<msg::PcmChunk> chunk, double duration) { chunkEncoded(encoder, chunk, duration); },
                   sampleFormat_);
    active_ = true;
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
        LOG(INFO, LOG_TAG) << "State changed: " << name_ << ", state: " << static_cast<int>(state_) << " => " << static_cast<int>(newState) << "\n";
        state_ = newState;
        for (auto* listener : pcmListeners_)
        {
            if (listener)
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
        if (listener)
            listener->onChunkEncoded(this, chunk, duration);
    }
}


void PcmStream::chunkRead(const msg::PcmChunk& chunk)
{
    for (auto* listener : pcmListeners_)
    {
        if (listener)
            listener->onChunkRead(this, chunk);
    }
    encoder_->encode(chunk);
}


void PcmStream::resync(const std::chrono::nanoseconds& duration)
{
    for (auto* listener : pcmListeners_)
    {
        if (listener)
            listener->onResync(this, duration.count() / 1000000.);
    }
}


json PcmStream::toJson() const
{
    string state("unknown");
    if (state_ == ReaderState::kIdle)
        state = "idle";
    else if (state_ == ReaderState::kPlaying)
        state = "playing";
    else if (state_ == ReaderState::kDisabled)
        state = "disabled";

    json j = {
        {"uri", uri_.toJson()},
        {"id", getId()},
        {"status", state},
    };

    if (meta_)
        j["meta"] = meta_->msg;

    return j;
}


void PcmStream::addListener(PcmListener* pcmListener)
{
    pcmListeners_.push_back(pcmListener);
}


std::shared_ptr<msg::StreamTags> PcmStream::getMeta() const
{
    return meta_;
}


void PcmStream::setMeta(const json& jtag)
{
    meta_.reset(new msg::StreamTags(jtag));
    meta_->msg["STREAM"] = name_;
    LOG(INFO, LOG_TAG) << "Stream: " << name_ << ", metadata=" << meta_->msg.dump(4) << "\n";

    // Trigger a stream update
    for (auto* listener : pcmListeners_)
    {
        if (listener)
            listener->onMetaChanged(this);
    }
}

} // namespace streamreader
