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

#ifndef NULL_ENCODER_HPP
#define NULL_ENCODER_HPP
#include "encoder.hpp"

namespace encoder
{

/// Null Encoder class
/**
 * Dummy encoder that will not encode any PcmChunk and thus will also never call
 * "Encoder::encoded_callback_", i.e. the "OnEncodedCallback" to report any encoded data
 * to the listener.
 * Typically used as input stream for a MetaStream, which will do it's own encoding.
 * Streams that use the null encoder cannot be listened to directly, nor they are visible
 * on the RPC interface.
 */
class NullEncoder : public Encoder
{
public:
    NullEncoder(const std::string& codecOptions = "");
    void encode(const msg::PcmChunk& chunk) override;
    std::string name() const override;

protected:
    void initEncoder() override;
};

} // namespace encoder

#endif
