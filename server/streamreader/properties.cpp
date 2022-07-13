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

#include "properties.hpp"

static constexpr auto LOG_TAG = "Properties";

namespace
{
template <typename T>
void readTag(const json& j, const std::string& tag, std::optional<T>& dest)
{
    try
    {
        if (!j.contains(tag))
            dest = std::nullopt;
        else
            dest = j[tag].get<T>();
    }
    catch (const std::exception& e)
    {
        LOG(ERROR, LOG_TAG) << "failed to read tag: '" << tag << "': " << e.what() << '\n';
    }
}

template <typename T>
void readTag(const json& j, const std::string& tag, T& dest, const T& def)
{
    std::optional<T> val;
    readTag(j, tag, val);
    if (val.has_value())
        dest = val.value();
    else
        dest = def;
}

template <typename T>
void addTag(json& j, const std::string& tag, const T& source)
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

template <typename T>
void addTag(json& j, const std::string& tag, const std::optional<T>& source)
{
    if (!source.has_value())
    {
        if (j.contains(tag))
            j.erase(tag);
    }
    else
        addTag(j, tag, source.value());
}
} // namespace


Properties::Properties(const json& j)
{
    fromJson(j);
}


json Properties::toJson() const
{
    json j;
    if (playback_status.has_value())
        addTag(j, "playbackStatus", std::optional<std::string>(to_string(playback_status.value())));
    if (loop_status.has_value())
        addTag(j, "loopStatus", std::optional<std::string>(to_string(loop_status.value())));
    addTag(j, "rate", rate);
    addTag(j, "shuffle", shuffle);
    addTag(j, "volume", volume);
    addTag(j, "mute", mute);
    addTag(j, "position", position);
    addTag(j, "minimumRate", minimum_rate);
    addTag(j, "maximumRate", maximum_rate);
    addTag(j, "canGoNext", can_go_next);
    addTag(j, "canGoPrevious", can_go_previous);
    addTag(j, "canPlay", can_play);
    addTag(j, "canPause", can_pause);
    addTag(j, "canSeek", can_seek);
    addTag(j, "canControl", can_control);
    if (metadata.has_value())
        addTag(j, "metadata", metadata->toJson());
    return j;
}

void Properties::fromJson(const json& j)
{
    static std::set<std::string> rw_props = {"loopStatus", "shuffle", "volume", "mute", "rate"};
    static std::set<std::string> ro_props = {"playbackStatus", "loopStatus",    "shuffle", "volume",   "mute",    "position",   "minimumRate", "maximumRate",
                                             "canGoNext",      "canGoPrevious", "canPlay", "canPause", "canSeek", "canControl", "metadata"};
    for (const auto& element : j.items())
    {
        bool is_rw = (rw_props.find(element.key()) != rw_props.end());
        bool is_ro = (ro_props.find(element.key()) != ro_props.end());
        if (!is_rw && !is_ro)
            LOG(WARNING, LOG_TAG) << "Property not supoorted: " << element.key() << "\n";
    }

    std::optional<std::string> opt;

    readTag(j, "playbackStatus", opt);
    if (opt.has_value())
        playback_status = playback_status_from_string(opt.value());
    else
        playback_status = std::nullopt;

    readTag(j, "loopStatus", opt);
    if (opt.has_value())
        loop_status = loop_status_from_string(opt.value());
    else
        loop_status = std::nullopt;
    readTag(j, "rate", rate);
    readTag(j, "shuffle", shuffle);
    readTag(j, "volume", volume);
    readTag(j, "mute", mute);
    readTag(j, "position", position);
    readTag(j, "minimumRate", minimum_rate);
    readTag(j, "maximumRate", maximum_rate);
    readTag(j, "canGoNext", can_go_next, false);
    readTag(j, "canGoPrevious", can_go_previous, false);
    readTag(j, "canPlay", can_play, false);
    readTag(j, "canPause", can_pause, false);
    readTag(j, "canSeek", can_seek, false);
    readTag(j, "canControl", can_control, false);

    if (j.contains("metadata"))
    {
        Metadata m;
        m.fromJson(j["metadata"]);
        metadata = m;
    }
    else
        metadata = std::nullopt;
}

bool Properties::operator==(const Properties& other) const
{
    // expensive, but not called ofetn and less typing
    return (toJson() == other.toJson());

    // clang-format off
        // return (playback_status == other.playback_status &&
        //         loop_status == other.loop_status &&
        //         rate == other.rate &&
        //         shuffle == other.shuffle &&
        //         volume == other.volume &&
        //         position == other.position &&
        //         minimum_rate == other.minimum_rate &&
        //         maximum_rate == other.maximum_rate &&
        //         can_go_next == other.can_go_next &&
        //         can_go_previous == other.can_go_previous &&
        //         can_play == other.can_play &&
        //         can_pause == other.can_pause &&
        //         can_seek == other.can_seek &&
        //         can_control == other.can_control);
    // clang-format on
}
