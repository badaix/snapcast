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


// standard headers
#include <string>

/// Daemonize a process: run a fork as specified user/group
class Daemon
{
public:
    /// c'tor
    Daemon(std::string user, std::string group, std::string pidfile);
    /// d'tor
    virtual ~Daemon();

    /// daemonize the process
    void daemonize();

private:
    int pidFilehandle_;
    std::string user_;
    std::string group_;
    std::string pidfile_;
};
