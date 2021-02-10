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

#include "time_provider.hpp"
#include "common/aixlog.hpp"

#include <chrono>

static constexpr auto LOG_TAG = "TimeProvider";

TimeProvider::TimeProvider() : diffToServer_(0)
{
    diffBuffer_.setSize(200);
}


void TimeProvider::setDiff(const tv& c2s, const tv& s2c)
{
    //	tv latency = c2s - s2c;
    //	double diff = (latency.sec * 1000. + latency.usec / 1000.) / 2.;
    double diff = (static_cast<double>(c2s.sec) / 2. - static_cast<double>(s2c.sec) / 2.) * 1000. +
                  (static_cast<double>(c2s.usec) / 2. - static_cast<double>(s2c.usec) / 2.) / 1000.;
    setDiffToServer(diff);
}


void TimeProvider::setDiffToServer(double ms)
{
    using namespace std::chrono_literals;
    auto now = std::chrono::system_clock::now();
    static auto lastTimeSync = now;
    auto diff = chronos::abs(now - lastTimeSync);

    /// clear diffBuffer if last update is older than a minute
    if (!diffBuffer_.empty() && (diff > 60s))
    {
        LOG(INFO, LOG_TAG) << "Last time sync older than a minute. Clearing time buffer\n";
        diffToServer_ = static_cast<chronos::usec::rep>(ms * 1000);
        diffBuffer_.clear();
    }
    lastTimeSync = now;

    diffBuffer_.add(static_cast<chronos::usec::rep>(ms * 1000));
    diffToServer_ = diffBuffer_.median();
    // LOG(INFO, LOG_TAG) << "setDiffToServer: " << ms << ", diff: " << diffToServer_ / 1000000 << " s, " << (diffToServer_ / 1000) % 1000 << "." <<
    // diffToServer_ % 1000 << " ms\n";
}

/*
long TimeProvider::getPercentileDiffToServer(size_t percentile)
{
        return diffBuffer.percentile(percentile);
}
*/
