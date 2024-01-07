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

#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

// local headers
#include "string_utils.hpp"

// 3rd party headers

// standard headers
#ifndef WINDOWS
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>


namespace utils
{
namespace file
{


static bool exists(const std::string& filename)
{
    return std::filesystem::exists(filename);
}


#ifndef WINDOWS
static void do_chown(const std::string& file_path, const std::string& user_name, const std::string& group_name)
{
    if (user_name.empty() && group_name.empty())
        return;

    if (!utils::file::exists(file_path))
        return;

    uid_t uid = -1;
    gid_t gid = -1;

    if (!user_name.empty())
    {
        struct passwd* pwd = getpwnam(user_name.c_str());
        if (pwd == nullptr)
            throw std::runtime_error("Failed to get uid");
        uid = pwd->pw_uid;
    }

    if (!group_name.empty())
    {
        struct group* grp = getgrnam(group_name.c_str());
        if (grp == nullptr)
            throw std::runtime_error("Failed to get gid");
        gid = grp->gr_gid;
    }

    if (chown(file_path.c_str(), uid, gid) == -1)
        throw std::runtime_error("chown failed");
}


static int mkdirRecursive(const char* path, mode_t mode)
{
    std::vector<std::string> pathes = utils::string::split(path, '/');
    std::stringstream ss;
    int res = 0;
    for (const auto& p : pathes)
    {
        if (p.empty())
            continue;
        ss << "/" << p;
        res = mkdir(ss.str().c_str(), mode);
        if ((res != 0) && (errno != EEXIST))
            return res;
    }
    return res;
}
#endif
} // namespace file
} // namespace utils

#endif
