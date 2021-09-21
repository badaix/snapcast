/***
    This file is part of snapcast
    Copyright (C) 2021  Manuel Weichselbaumer

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

#ifndef SOFTWARE_DSP_HPP
#define SOFTWARE_DSP_HPP

#include "common/endian.hpp"
#include "common/sample_format.hpp"

namespace player
{

/// Software DSP
/**
 * Software DSP to handle all sample processing like volume or equalizer.
 */
class SoftwareDsp
{
public:
    SoftwareDsp(const SampleFormat& out_format);
    virtual ~SoftwareDsp();

    /// process buffer to apply volume
    /// @param buffer the raw data to process
    /// @param frames the number of frames to process
    /// @param volume the volume to apply
    void adjustVolume(char* buffer, size_t frames, double volume);

private:
    SampleFormat out_format_;

    template <typename T>
    static void adjustVolume(char* buffer, size_t count, double volume)
    {
        T* bufferT = (T*)buffer;
        for (size_t n = 0; n < count; ++n)
            bufferT[n] = endian::swap<T>(static_cast<T>(endian::swap<T>(bufferT[n]) * volume));
    }
};

} // namespace player

#endif // SOFTWARE_DSP_HPP
