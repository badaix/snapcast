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

#include "software_dsp.hpp"

namespace player
{

SoftwareDsp::SoftwareDsp(const SampleFormat& out_format) : out_format_(out_format)
{
}

SoftwareDsp::~SoftwareDsp()
{
}

void SoftwareDsp::adjustVolume(char* buffer, size_t frames, double volume)
{
    if (volume != 1.0)
    {
        if (out_format_.sampleSize() == 1)
            adjustVolume<int8_t>(buffer, frames * out_format_.channels(), volume);
        else if (out_format_.sampleSize() == 2)
            adjustVolume<int16_t>(buffer, frames * out_format_.channels(), volume);
        else if (out_format_.sampleSize() == 4)
            adjustVolume<int32_t>(buffer, frames * out_format_.channels(), volume);
    }
}

} // namespace player
