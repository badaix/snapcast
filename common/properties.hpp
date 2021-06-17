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

#ifndef PROPERTIES_HPP
#define PROPERTIES_HPP

#include <set>
#include <string>

#include <boost/optional.hpp>

#include "common/aixlog.hpp"
#include "common/json.hpp"

using json = nlohmann::json;

enum class PlaybackStatus
{
    kPlaying = 0,
    kPaused = 1,
    kStopped = 2,
    kUnknown = 3
};


static std::string to_string(PlaybackStatus playback_status)
{
    switch (playback_status)
    {
        case PlaybackStatus::kPlaying:
            return "playing";
        case PlaybackStatus::kPaused:
            return "paused";
        case PlaybackStatus::kStopped:
            return "stopped";
        default:
            return "unknown";
    }
}


static std::ostream& operator<<(std::ostream& os, PlaybackStatus playback_status)
{
    os << to_string(playback_status);
    return os;
}


static PlaybackStatus playback_status_from_string(std::string& status)
{
    if (status == "playing")
        return PlaybackStatus::kPlaying;
    else if (status == "paused")
        return PlaybackStatus::kPaused;
    else if (status == "stopped")
        return PlaybackStatus::kStopped;
    else
        return PlaybackStatus::kUnknown;
}


static std::istream& operator>>(std::istream& is, PlaybackStatus& playback_status)
{
    std::string status;
    playback_status = PlaybackStatus::kUnknown;
    if (is >> status)
        playback_status = playback_status_from_string(status);
    else
        playback_status = PlaybackStatus::kUnknown;
    return is;
}


enum class LoopStatus
{
    kNone = 0,
    kTrack = 1,
    kPlaylist = 2,
    kUnknown = 3
};


static std::string to_string(LoopStatus loop_status)
{
    switch (loop_status)
    {
        case LoopStatus::kNone:
            return "none";
        case LoopStatus::kTrack:
            return "track";
        case LoopStatus::kPlaylist:
            return "playlist";
        default:
            return "unknown";
    }
}


static std::ostream& operator<<(std::ostream& os, LoopStatus loop_status)
{
    os << to_string(loop_status);
    return os;
}


static LoopStatus loop_status_from_string(std::string& status)
{
    if (status == "none")
        return LoopStatus::kNone;
    else if (status == "track")
        return LoopStatus::kTrack;
    else if (status == "playlist")
        return LoopStatus::kPlaylist;
    else
        return LoopStatus::kUnknown;
}


static std::istream& operator>>(std::istream& is, LoopStatus& loop_status)
{
    std::string status;
    if (is >> status)
        loop_status = loop_status_from_string(status);
    else
        loop_status = LoopStatus::kUnknown;

    return is;
}


class Properties
{
public:
    Properties() = default;
    Properties(const json& j)
    {
        fromJson(j);
    }

    /// https://www.musicpd.org/doc/html/protocol.html#tags
    /// The current playback status
    PlaybackStatus playback_status;
    /// The current loop / repeat status
    boost::optional<LoopStatus> loop_status;
    /// The current playback rate
    float rate;
    /// A value of false indicates that playback is progressing linearly through a playlist, while true means playback is progressing through a playlist in some
    /// other order.
    boost::optional<bool> shuffle;
    /// The volume level between 0-100
    boost::optional<int> volume;
    /// The current track position in seconds
    boost::optional<float> position;
    /// The minimum value which the Rate property can take. Clients should not attempt to set the Rate property below this value
    boost::optional<float> minimum_rate;
    /// The maximum value which the Rate property can take. Clients should not attempt to set the Rate property above this value
    boost::optional<float> maximum_rate;
    /// Whether the client can call the Next method on this interface and expect the current track to change
    boost::optional<bool> can_go_next;
    /// Whether the client can call the Previous method on this interface and expect the current track to change
    boost::optional<bool> can_go_previous;
    /// Whether playback can be started using "play" or "playPause"
    boost::optional<bool> can_play;
    /// Whether playback can be paused using "pause" or "playPause"
    boost::optional<bool> can_pause;
    /// Whether the client can control the playback position using "seek" and "setPosition". This may be different for different tracks
    boost::optional<bool> can_seek;
    /// Whether the media player may be controlled over this interface
    boost::optional<bool> can_control;

    json toJson() const
    {
        json j;
        addTag(j, "playbackStatus", to_string(playback_status));
        if (loop_status.has_value())
            addTag(j, "loopStatus", boost::optional<std::string>(to_string(loop_status.value())));
        addTag(j, "rate", rate);
        addTag(j, "shuffle", shuffle);
        addTag(j, "volume", volume);
        addTag(j, "position", position);
        addTag(j, "minimumRate", minimum_rate);
        addTag(j, "maximumRate", maximum_rate);
        addTag(j, "canGoNext", can_go_next);
        addTag(j, "canGoPrevious", can_go_previous);
        addTag(j, "canPlay", can_play);
        addTag(j, "canPause", can_pause);
        addTag(j, "canSeek", can_seek);
        addTag(j, "canControl", can_control);
        return j;
    }

    void fromJson(const json& j)
    {
        static std::set<std::string> rw_props = {"loopStatus", "shuffle", "volume", "rate"};
        static std::set<std::string> ro_props = {"playbackStatus", "loopStatus",    "shuffle", "volume",   "position", "minimumRate", "maximumRate",
                                                 "canGoNext",      "canGoPrevious", "canPlay", "canPause", "canSeek",  "canControl"};
        for (const auto& element : j.items())
        {
            bool is_rw = (rw_props.find(element.key()) != rw_props.end());
            bool is_ro = (ro_props.find(element.key()) != ro_props.end());
            if (!is_rw && !is_ro)
                LOG(WARNING, LOG_TAG) << "Property not supoorted: " << element.key() << "\n";
        }

        boost::optional<std::string> opt;

        readTag(j, "playbackStatus", opt);
        if (!opt.has_value())
            playback_status = PlaybackStatus::kStopped;
        else
            playback_status = playback_status_from_string(opt.value());

        readTag(j, "loopStatus", opt);
        if (opt.has_value())
            loop_status = loop_status_from_string(opt.value());
        else
            loop_status = boost::none;
        readTag(j, "rate", rate, 1.0f);
        readTag(j, "shuffle", shuffle);
        readTag(j, "volume", volume);
        readTag(j, "position", position);
        readTag(j, "minimumRate", minimum_rate);
        readTag(j, "maximumRate", maximum_rate);
        readTag(j, "canGoNext", can_go_next);
        readTag(j, "canGoPrevious", can_go_previous);
        readTag(j, "canPlay", can_play);
        readTag(j, "canPause", can_pause);
        readTag(j, "canSeek", can_seek);
        readTag(j, "canControl", can_control);
    }

    bool operator==(const Properties& other) const
    {
        // expensive, but not called ofetn and less typing
        return (toJson() == other.toJson());
    }

private:
    template <typename T>
    void readTag(const json& j, const std::string& tag, boost::optional<T>& dest) const
    {
        try
        {
            if (!j.contains(tag))
                dest = boost::none;
            else
                dest = j[tag].get<T>();
        }
        catch (const std::exception& e)
        {
            LOG(ERROR, LOG_TAG) << "failed to read tag: '" << tag << "': " << e.what() << '\n';
        }
    }

    template <typename T>
    void readTag(const json& j, const std::string& tag, T& dest, const T& def) const
    {
        boost::optional<T> val;
        readTag(j, tag, val);
        if (val.has_value())
            dest = val.value();
        else
            dest = def;
    }

    template <typename T>
    void addTag(json& j, const std::string& tag, const boost::optional<T>& source) const
    {
        if (!source.has_value())
        {
            if (j.contains(tag))
                j.erase(tag);
        }
        else
            addTag(j, tag, source.value());
    }

    template <typename T>
    void addTag(json& j, const std::string& tag, const T& source) const
    {
        try
        {
            j[tag] = source;
        }
        catch (const std::exception& e)
        {
            LOG(ERROR, LOG_TAG) << "failed to add tag: '" << tag << "': " << e.what() << '\n';
        }
    }

private:
    static constexpr auto LOG_TAG = "Properties";
};


#endif
