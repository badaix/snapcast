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

#ifndef META_STREAM_HPP
#define META_STREAM_HPP

#include "posix_stream.hpp"
#include "resampler.hpp"
#include <memory>

namespace streamreader
{


/// Reads and decodes PCM data
/**
 * Reads PCM and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class MetaStream : public PcmStream, public PcmListener
{
public:
    /// ctor. Encoded PCM data is passed to the PcmListener
    MetaStream(PcmListener* pcmListener, const std::vector<std::shared_ptr<PcmStream>>& streams, boost::asio::io_context& ioc,
               const ServerSettings& server_settings, const StreamUri& uri);
    virtual ~MetaStream();

    void start() override;
    void stop() override;

protected:
    /// Implementation of PcmListener
    void onMetaChanged(const PcmStream* pcmStream) override;
    void onStateChanged(const PcmStream* pcmStream, ReaderState state) override;
    void onChunkRead(const PcmStream* pcmStream, const msg::PcmChunk& chunk) override;
    void onChunkEncoded(const PcmStream* pcmStream, std::shared_ptr<msg::PcmChunk> chunk, double duration) override;
    void onResync(const PcmStream* pcmStream, double ms) override;

protected:
    std::vector<std::shared_ptr<PcmStream>> streams_;
    std::shared_ptr<PcmStream> active_stream_;
    std::mutex mutex_;
    std::unique_ptr<Resampler> resampler_;
    bool first_read_;
    std::chrono::time_point<std::chrono::steady_clock> next_tick_;
};

} // namespace streamreader

#endif
