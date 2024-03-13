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

#ifndef META_STREAM_HPP
#define META_STREAM_HPP

// local headers
#include "common/resampler.hpp"
#include "pcm_stream.hpp"

// standard headers
#include <memory>

namespace streamreader
{

// Mixing digital audio:
// https://www.vttoth.com/CMS/technical-notes/?view=article&id=68

/// Reads and decodes PCM data
/**
 * Reads PCM and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmStream::Listener
 */
class MetaStream : public PcmStream, public PcmStream::Listener
{
public:
    /// ctor. Encoded PCM data is passed to the PcmStream::Listener
    MetaStream(PcmStream::Listener* pcmListener, const std::vector<std::shared_ptr<PcmStream>>& streams, boost::asio::io_context& ioc,
               const ServerSettings& server_settings, const StreamUri& uri);
    virtual ~MetaStream();

    void start() override;
    void stop() override;

    // Setter for properties
    void setShuffle(bool shuffle, ResultHandler handler) override;
    void setLoopStatus(LoopStatus status, ResultHandler handler) override;
    void setVolume(uint16_t volume, ResultHandler handler) override;
    void setMute(bool mute, ResultHandler handler) override;
    void setRate(float rate, ResultHandler handler) override;

    // Control commands
    void setPosition(std::chrono::milliseconds position, ResultHandler handler) override;
    void seek(std::chrono::milliseconds offset, ResultHandler handler) override;
    void next(ResultHandler handler) override;
    void previous(ResultHandler handler) override;
    void pause(ResultHandler handler) override;
    void playPause(ResultHandler handler) override;
    void stop(ResultHandler handler) override;
    void play(ResultHandler handler) override;

protected:
    /// Implementation of PcmStream::Listener
    void onPropertiesChanged(const PcmStream* pcmStream, const Properties& properties) override;
    void onStateChanged(const PcmStream* pcmStream, ReaderState state) override;
    void onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk) override;
    void onChunkEncoded(const PcmStream* pcmStream, std::shared_ptr<msg::PcmChunk> chunk, double duration) override;
    void onResync(const PcmStream* pcmStream, double ms) override;

protected:
    std::vector<std::shared_ptr<PcmStream>> streams_;
    std::recursive_mutex active_mutex_;
    std::shared_ptr<PcmStream> active_stream_;
    std::unique_ptr<Resampler> resampler_;
    bool first_read_;
    std::chrono::time_point<std::chrono::steady_clock> next_tick_;
};

} // namespace streamreader

#endif
