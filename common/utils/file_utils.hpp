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

// 3rd party headers

// standard headers
#include <string>

#ifndef WINDOWS
#include <filesystem>
#include <optional>
#include <sys/types.h> // mode_t
#endif

namespace utils::file
{

/// @return true if @p filename exists
bool exists(const std::string& filename);

#ifndef WINDOWS
/// @return absolute version of @p filename, if @p filename is located in @p directory
std::optional<std::filesystem::path> isInDirectory(std::filesystem::path filename, std::filesystem::path directory);

/// change owner if @p file_path to user @p user_name and group @p group_name
void do_chown(const std::string& file_path, const std::string& user_name, const std::string& group_name);
/// make recursice directory
int mkdirRecursive(const char* path, mode_t mode);
#endif

} // namespace utils::file
