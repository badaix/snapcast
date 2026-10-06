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
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

namespace streamreader
{

/// Progress of the current track, extracted from a Shairport-sync "prgr" (progress) metadata item
struct AirplayProgress
{
    /// elapsed time of the current track [s]
    float position;
    /// total duration of the current track [s]
    float duration;
};

/// Parse the payload of a Shairport-sync "prgr" (progress) metadata item.
/// The payload is of the form "start/current/end", where the three values are
/// RTP timestamps in frames at 44100 frames per second.
/// @param data the decoded payload of the "prgr" item
/// @return the progress, or nullopt if @p data is malformed
inline std::optional<AirplayProgress> parseAirplayProgress(const std::string& data)
{
    static constexpr double frames_per_second = 44100.;

    uint64_t timestamps[3] = {0, 0, 0};
    const char* ptr = data.c_str();
    for (size_t i = 0; i < 3; ++i)
    {
        if ((*ptr < '0') || (*ptr > '9'))
            return std::nullopt;
        char* end = nullptr;
        timestamps[i] = strtoull(ptr, &end, 10);
        ptr = end;
        if ((i < 2) && (*ptr++ != '/'))
            return std::nullopt;
    }
    if (*ptr != '\0')
        return std::nullopt;

    const uint64_t start = timestamps[0];
    const uint64_t current = timestamps[1];
    const uint64_t end = timestamps[2];
    if (end <= start)
        return std::nullopt;

    AirplayProgress progress;
    progress.duration = static_cast<float>(static_cast<double>(end - start) / frames_per_second);
    // The current play point can lag slightly behind the start of the play sequence
    progress.position = (current > start) ? static_cast<float>(static_cast<double>(current - start) / frames_per_second) : 0.f;
    return progress;
}

} // namespace streamreader
