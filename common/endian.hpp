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

// standard headers
#include <cstdint>

#ifdef IS_BIG_ENDIAN
#define SWAP_16(x) (__builtin_bswap16(x))
#define SWAP_32(x) (__builtin_bswap32(x))
#define SWAP_64(x) (__builtin_bswap64(x))
#else
#define SWAP_16(x) x
#define SWAP_32(x) x
#define SWAP_64(x) x
#endif

namespace endian
{

template <typename T>
T swap(const T&);

template <>
inline int8_t swap(const int8_t& val)
{
    return val;
}

template <>
inline int16_t swap(const int16_t& val)
{
    return SWAP_16(val);
}

template <>
inline int32_t swap(const int32_t& val)
{
    return SWAP_32(val);
}

template <>
inline int64_t swap(const int64_t& val)
{
    return SWAP_64(val);
}
} // namespace endian
