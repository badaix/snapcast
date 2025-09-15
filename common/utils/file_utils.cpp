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

// prototype/interface header file
#include "file_utils.hpp"

// local headers
#include "common/aixlog.hpp"

// local headers
#include "string_utils.hpp"

// 3rd party headers

// standard headers
#ifndef WINDOWS
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <stdexcept>
#include <vector>
#endif
#include <filesystem>


namespace utils::file
{

static constexpr auto LOG_TAG = "FileUtils";


bool exists(const std::string& filename)
{
    return std::filesystem::exists(filename);
}


#ifndef WINDOWS
std::optional<std::filesystem::path> isInDirectory(std::filesystem::path filename, std::filesystem::path directory)
{
    auto addLeadingSlash = [](const std::filesystem::path& path) -> std::filesystem::path
    { return path.native() + std::filesystem::path::preferred_separator; };
    // convert directory to normalized absolute path with trailing slash
    directory = addLeadingSlash(std::filesystem::weakly_canonical(directory));
    // if exe file name is relative, prepend the sandbox_dir
    if (!filename.is_absolute())
        filename = directory / filename;
    // convert filename to normalized absolute path
    filename = std::filesystem::weakly_canonical(filename);
    // check if file is located in directory
    bool res = (addLeadingSlash(filename.parent_path()).native().find(directory.native()) == 0);
    LOG(DEBUG, LOG_TAG) << "isInDirectory: '" << addLeadingSlash(filename.parent_path()).native() << "', directory: '" << directory.native()
                        << "', res: " << res << "\n";
    LOG(DEBUG, LOG_TAG) << "isInDirectory: '" << filename << "', directory: '" << directory << "', res: " << res << "\n";
    if (res)
        return filename;
    return std::nullopt;
}


void do_chown(const std::string& file_path, const std::string& user_name, const std::string& group_name)
{
    if (user_name.empty() && group_name.empty())
        return;

    if (!utils::file::exists(file_path))
        return;

    uid_t uid = -1;
    gid_t gid = -1;

    if (!user_name.empty())
    {
        const struct passwd* pwd = getpwnam(user_name.c_str());
        if (pwd == nullptr)
            throw std::runtime_error("Failed to get uid");
        uid = pwd->pw_uid;
    }

    if (!group_name.empty())
    {
        const struct group* grp = getgrnam(group_name.c_str());
        if (grp == nullptr)
            throw std::runtime_error("Failed to get gid");
        gid = grp->gr_gid;
    }

    if (chown(file_path.c_str(), uid, gid) == -1)
        throw std::runtime_error("chown failed");
}


int mkdirRecursive(const char* path, mode_t mode)
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

} // namespace utils::file
