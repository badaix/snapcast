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

#include "meta_stream.hpp"
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/utils/string_utils.hpp"
#include "encoder/encoder_factory.hpp"


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "MetaStream";
// static constexpr auto kResyncTolerance = 50ms;


MetaStream::MetaStream(PcmListener* pcmListener, const std::vector<std::shared_ptr<PcmStream>>& streams, boost::asio::io_context& ioc, const StreamUri& uri)
    : PcmStream(pcmListener, ioc, uri), first_read_(true)
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

    if (!streams_.empty())
    {
        active_stream_ = streams_.front();
        resampler_ = make_unique<Resampler>(active_stream_->getSampleFormat(), sampleFormat_);
    }
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


void MetaStream::onMetaChanged(const PcmStream* pcmStream)
{
    LOG(DEBUG, LOG_TAG) << "onMetaChanged: " << pcmStream->getName() << "\n";
    std::lock_guard<std::mutex> lock(mutex_);
    if (pcmStream != active_stream_.get())
        return;
}


void MetaStream::onStateChanged(const PcmStream* pcmStream, ReaderState state)
{
    LOG(DEBUG, LOG_TAG) << "onStateChanged: " << pcmStream->getName() << ", state: " << state << "\n";
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& stream : streams_)
    {
        if (stream->getState() == ReaderState::kPlaying)
        {
            if (state_ != ReaderState::kPlaying) // || (active_stream_ != stream))
                first_read_ = true;

            if (active_stream_ != stream)
            {
                LOG(INFO, LOG_TAG) << "Stream: " << name_ << ", switching active stream: " << (active_stream_ ? active_stream_->getName() : "<null>") << " => "
                                   << stream->getName() << "\n";
                active_stream_ = stream;
                resampler_ = make_unique<Resampler>(active_stream_->getSampleFormat(), sampleFormat_);
            }

            setState(ReaderState::kPlaying);
            return;
        }
    }
    active_stream_ = nullptr;
    setState(ReaderState::kIdle);
}


void MetaStream::onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk)
{
    // LOG(TRACE, LOG_TAG) << "onChunkRead: " << pcmStream->getName() << ", duration: " << chunk.durationMs() << "\n";
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mutex_);
    if (pcmStream != active_stream_.get())
        return;
    resync(std::chrono::nanoseconds(static_cast<int64_t>(ms * 1000000)));
}


} // namespace streamreader
