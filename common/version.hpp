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

#ifndef VERSION_HPP
#define VERSION_HPP

#include <string>

namespace version
{

#ifdef REVISION
static constexpr auto revision = REVISION;
#else
static constexpr auto revision = "";
#endif

#ifdef VERSION
static constexpr auto code = VERSION;
#else
static constexpr auto code = "";
#endif

static std::string rev(std::size_t len = 0)
{
    if (len == 0)
    {
        return revision;
    }
    return std::string(revision).substr(0, len);
}

} // namespace version

#endif
