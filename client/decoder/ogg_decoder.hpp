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

#pragma once

// local headers
#include "decoder.hpp"

// 3rd party headers
#ifdef HAS_TREMOR
#include <tremor/ivorbiscodec.h>
#else
#include <vorbis/codec.h>
#endif
#include <ogg/ogg.h>

// standard headers
#include <mutex>


namespace decoder
{

class OggDecoder : public Decoder
{
public:
    OggDecoder();
    ~OggDecoder() override;
    bool decode(msg::PcmChunk* chunk) override;
    SampleFormat setHeader(msg::CodecHeader* chunk) override;

private:
    bool decodePayload(msg::PcmChunk* chunk);
    template <typename T, typename IN_TYPE>
    T clip(const IN_TYPE& value, const T& lower, const T& upper) const
    {
        auto val = static_cast<int64_t>(value);
        if (val > upper)
            return upper;
        if (val < lower)
            return lower;
        return static_cast<T>(value);
    }

    ogg_sync_state oy;   /// sync and verify incoming physical bitstream
    ogg_stream_state os; /// take physical pages, weld into a logical stream of packets
    ogg_page og;         /// one Ogg bitstream page. Vorbis packets are inside
    ogg_packet op;       /// one raw packet of data for decode

    vorbis_info vi;      /// struct that stores all the static vorbis bitstream settings
    vorbis_comment vc;   /// struct that stores all the bitstream user comments
    vorbis_dsp_state vd; /// central working state for the packet->PCM decoder
    vorbis_block vb;     /// local working space for packet->PCM decode

    SampleFormat sampleFormat_;
    std::mutex mutex_;
};

} // namespace decoder
