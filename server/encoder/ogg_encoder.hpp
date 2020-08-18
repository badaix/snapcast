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

#ifndef OGG_ENCODER_HPP
#define OGG_ENCODER_HPP
#include "encoder.hpp"
#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>

namespace encoder
{

class OggEncoder : public Encoder
{
public:
    OggEncoder(const std::string& codecOptions = "");
    ~OggEncoder() override;

    void encode(const msg::PcmChunk& chunk) override;
    std::string getAvailableOptions() const override;
    std::string getDefaultOptions() const override;
    std::string name() const override;

protected:
    void initEncoder() override;

private:
    ogg_stream_state os_; /// take physical pages, weld into a logical stream of packets
    ogg_page og_;         /// one Ogg bitstream page.  Vorbis packets are inside
    ogg_packet op_;       /// one raw packet of data for decode

    vorbis_info vi_;    /// struct that stores all the static vorbis bitstream settings
    vorbis_comment vc_; /// struct that stores all the user comments

    vorbis_dsp_state vd_; /// central working state for the packet->PCM decoder
    vorbis_block vb_;     /// local working space for packet->PCM decode

    ogg_int64_t lastGranulepos_;
};

} // namespace encoder

#endif
