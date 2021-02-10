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

#include "encoder_factory.hpp"
#include "null_encoder.hpp"
#include "pcm_encoder.hpp"
#if defined(HAS_OGG) && defined(HAS_VORBIS) && defined(HAS_VORBIS_ENC)
#include "ogg_encoder.hpp"
#endif
#if defined(HAS_FLAC)
#include "flac_encoder.hpp"
#endif
#if defined(HAS_OPUS)
#include "opus_encoder.hpp"
#endif
#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/utils/string_utils.hpp"


using namespace std;

namespace encoder
{

std::unique_ptr<Encoder> EncoderFactory::createEncoder(const std::string& codecSettings) const
{
    std::string codec(codecSettings);
    std::string codecOptions;
    if (codec.find(':') != std::string::npos)
    {
        codecOptions = utils::string::trim_copy(codec.substr(codec.find(':') + 1));
        codec = utils::string::trim_copy(codec.substr(0, codec.find(':')));
    }
    if (codec == "pcm")
        return std::make_unique<PcmEncoder>(codecOptions);
    else if (codec == "null")
        return std::make_unique<NullEncoder>(codecOptions);
#if defined(HAS_OGG) && defined(HAS_VORBIS) && defined(HAS_VORBIS_ENC)
    else if (codec == "ogg")
        return std::make_unique<OggEncoder>(codecOptions);
#endif
#if defined(HAS_FLAC)
    else if (codec == "flac")
        return std::make_unique<FlacEncoder>(codecOptions);
#endif
#if defined(HAS_OPUS)
    else if (codec == "opus")
        return std::make_unique<OpusEncoder>(codecOptions);
#endif

    throw SnapException("unknown codec: " + codec);
}

} // namespace encoder
