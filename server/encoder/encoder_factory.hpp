/***
    This file is part of snapcast
    Copyright (C) 2014-2025  Johannes Pohl

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
#include "encoder.hpp"

// standard headers
#include <memory>
#include <string>


namespace encoder
{

/// Factory to create an encoder from an URI
class EncoderFactory
{
public:
    /// @return Encoder from @p codec_settings
    std::unique_ptr<Encoder> createEncoder(const std::string& codec_settings) const;
};

} // namespace encoder
