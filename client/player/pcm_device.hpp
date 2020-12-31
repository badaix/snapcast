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

#ifndef PCM_DEVICE_HPP
#define PCM_DEVICE_HPP

#include <string>

namespace player
{

static constexpr char DEFAULT_DEVICE[] = "default";

struct PcmDevice
{
    PcmDevice() : idx(-1), name(DEFAULT_DEVICE){};

    PcmDevice(int idx, const std::string& name, const std::string& description = "") : idx(idx), name(name), description(description){};

    int idx;
    std::string name;
    std::string description;
};

} // namespace player

#endif
