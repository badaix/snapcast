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

// prototype/interface header file
#include "meta_stream.hpp"

// local headers
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/utils/string_utils.hpp"
#include "encoder/encoder_factory.hpp"


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "MetaStream";
// static constexpr auto kResyncTolerance = 50ms;


MetaStream::MetaStream(PcmStream::Listener* pcmListener, const std::vector<std::shared_ptr<PcmStream>>& streams, boost::asio::io_context& ioc,
                       const ServerSettings& server_settings, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, server_settings, uri), first_read_(true)
{
    auto path_components = utils::string::split(uri.path, '/');
    for (const auto& component : path_components)
    {
        if (component.empty())
            continue;

        bool found = false;
        for (const auto& stream : streams)
        {
            if (stream->getName() == component)
            {
                streams_.push_back(stream);
                stream->addListener(this);
                found = true;
                break;
            }
        }
        if (!found)
            throw SnapException("Unknown stream: \"" + component + "\"");
    }

    if (streams_.empty())
        throw SnapException("Meta stream '" + getName() + "' must contain at least one stream");

    active_stream_ = streams_.front();
    resampler_ = make_unique<Resampler>(active_stream_->getSampleFormat(), sampleFormat_);
}


MetaStream::~MetaStream()
{
    stop();
}


void MetaStream::start()
{
    LOG(DEBUG, LOG_TAG) << "Start, sampleformat: " << sampleFormat_.toString() << "\n";
    PcmStream::start();
}


void MetaStream::stop()
{
    active_ = false;
}


void MetaStream::onPropertiesChanged(const PcmStream* pcmStream, const Properties& properties)
{
    LOG(DEBUG, LOG_TAG) << "onPropertiesChanged: " << pcmStream->getName() << "\n";
    // std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (pcmStream != active_stream_.get())
        return;
    setProperties(properties);
}


void MetaStream::onStateChanged(const PcmStream* pcmStream, ReaderState state)
{
    LOG(DEBUG, LOG_TAG) << "onStateChanged: " << pcmStream->getName() << ", state: " << state << "\n";
    std::lock_guard<std::recursive_mutex> lock(active_mutex_);

    // Should a pause keep the stream active? E.g. Spotify can only pause, so it would never get inactive
    // if (active_stream_->getProperties().playback_status == PlaybackStatus::kPaused)
    //     return;

    auto switch_stream = [this](std::shared_ptr<PcmStream> new_stream)
    {
        if (new_stream == active_stream_)
            return;
        LOG(INFO, LOG_TAG) << "Stream: " << name_ << ", switching active stream: " << (active_stream_ ? active_stream_->getName() : "<null>") << " => "
                           << new_stream->getName() << "\n";
        active_stream_ = new_stream;
        setProperties(active_stream_->getProperties());
        resampler_ = make_unique<Resampler>(active_stream_->getSampleFormat(), sampleFormat_);
    };

    for (const auto& stream : streams_)
    {
        if (stream->getState() == ReaderState::kPlaying)
        {
            if (state_ != ReaderState::kPlaying) // || (active_stream_ != stream))
                first_read_ = true;

            if (active_stream_ != stream)
            {
                switch_stream(stream);
            }

            setState(ReaderState::kPlaying);
            return;
        }
    }

    switch_stream(streams_.front());
    setState(ReaderState::kIdle);
}


void MetaStream::onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk)
{
    // LOG(TRACE, LOG_TAG) << "onChunkRead: " << pcmStream->getName() << ", duration: " << chunk.durationMs() << "\n";
    // std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::lock_guard<std::recursive_mutex> lock(active_mutex_);
    if (pcmStream != active_stream_.get())
        return;
    // active_stream_->sampleFormat_
    // sampleFormat_

    if (first_read_)
    {
        first_read_ = false;
        LOG(INFO, LOG_TAG) << "first read, updating timestamp\n";
        tvEncodedChunk_ = std::chrono::steady_clock::now() - chunk.duration<std::chrono::nanoseconds>();
        next_tick_ = std::chrono::steady_clock::now();
    }


    next_tick_ += chunk.duration<std::chrono::nanoseconds>();
    auto currentTick = std::chrono::steady_clock::now();
    auto next_read = next_tick_ - currentTick;

    // Read took longer, wait for the buffer to fill up
    if (next_read < 0ms)
    {
        // if (next_read >= -kResyncTolerance)
        // {
        //     LOG(INFO, LOG_TAG) << "next read < 0 (" << getName() << "): " << std::chrono::duration_cast<std::chrono::microseconds>(next_read).count() / 1000.
        //                        << " ms\n";
        // }
        // else
        // {
        resync(-next_read);
        first_read_ = true;
        // }
    }

    if (resampler_ && resampler_->resamplingNeeded())
    {
        auto resampled_chunk = resampler_->resample(chunk);
        if (resampled_chunk)
            chunkRead(*resampled_chunk);
    }
    else
        chunkRead(chunk);
}


void MetaStream::onChunkEncoded(const PcmStream* pcmStream, std::shared_ptr<msg::PcmChunk> chunk, double duration)
{
    std::ignore = pcmStream;
    std::ignore = chunk;
    std::ignore = duration;
    // LOG(TRACE, LOG_TAG) << "onChunkEncoded: " << pcmStream->getName() << ", duration: " << duration << "\n";
    // chunkEncoded(*encoder_, chunk, duration);
}


void MetaStream::onResync(const PcmStream* pcmStream, double ms)
{
    LOG(DEBUG, LOG_TAG) << "onResync: " << pcmStream->getName() << ", duration: " << ms << " ms\n";
    // std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (pcmStream != active_stream_.get())
        return;
    resync(std::chrono::nanoseconds(static_cast<int64_t>(ms * 1000000)));
}



// Setter for properties
void MetaStream::setShuffle(bool shuffle, ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->setShuffle(shuffle, std::move(handler));
}

void MetaStream::setLoopStatus(LoopStatus status, ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->setLoopStatus(status, std::move(handler));
}

void MetaStream::setVolume(uint16_t volume, ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->setVolume(volume, std::move(handler));
}

void MetaStream::setMute(bool mute, ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->setMute(mute, std::move(handler));
}

void MetaStream::setRate(float rate, ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->setRate(rate, std::move(handler));
}


// Control commands
void MetaStream::setPosition(std::chrono::milliseconds position, ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->setPosition(position, std::move(handler));
}

void MetaStream::seek(std::chrono::milliseconds offset, ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->seek(offset, std::move(handler));
}

void MetaStream::next(ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->next(std::move(handler));
}

void MetaStream::previous(ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->previous(std::move(handler));
}

void MetaStream::pause(ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->pause(std::move(handler));
}

void MetaStream::playPause(ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "PlayPause\n";
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (active_stream_->getState() == ReaderState::kIdle)
        play(handler);
    else
        active_stream_->playPause(std::move(handler));
}

void MetaStream::stop(ResultHandler handler)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    active_stream_->stop(std::move(handler));
}

void MetaStream::play(ResultHandler handler)
{
    LOG(DEBUG, LOG_TAG) << "Play\n";
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if ((active_stream_->getProperties().can_play) && (active_stream_->getProperties().playback_status != PlaybackStatus::kPlaying))
        return active_stream_->play(std::move(handler));

    for (const auto& stream : streams_)
    {
        if ((stream->getState() == ReaderState::kIdle) && (stream->getProperties().can_play))
        {
            return stream->play(std::move(handler));
        }
    }

    // call play on the active stream to get the handler called
    active_stream_->play(std::move(handler));
}


} // namespace streamreader
