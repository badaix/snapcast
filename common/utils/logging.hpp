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

#ifndef LOGGING_UTILS_HPP
#define LOGGING_UTILS_HPP

#include "common/aixlog.hpp"
#include <chrono>


namespace utils
{
namespace logging
{

using namespace std::chrono_literals;

/// Log Conditional to limit log frequency
struct TimeConditional : public AixLog::Conditional
{
    /// c'tor
    /// @param interval duration that must be passed since the last log line
    TimeConditional(const std::chrono::milliseconds& interval) : interval_(interval)
    {
        reset();
    }

    /// return true for the next check
    void reset()
    {
        last_time_ = std::chrono::steady_clock::now() - interval_ - 1s;
    }

    /// Change log interval
    /// @param interval duration that must be passed since the last log line
    void setInterval(const std::chrono::milliseconds& interval)
    {
        interval_ = interval;
    }

    /// check if the interval is passed
    /// @return true if interval passed since the last log
    bool is_true() const override
    {
        auto now = std::chrono::steady_clock::now();
        if (now > last_time_ + interval_)
        {
            last_time_ = now;
            return true;
        }
        return false;
    }

private:
    /// log interval
    std::chrono::milliseconds interval_;
    /// last log time
    mutable std::chrono::steady_clock::time_point last_time_;
};

} // namespace logging
} // namespace utils

#endif
