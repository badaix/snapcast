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

#ifndef METADATA_HPP
#define METADATA_HPP

#include "common/aixlog.hpp"
#include "common/json.hpp"

#include <optional>
#include <set>
#include <string>

using json = nlohmann::json;

class Metadata
{
public:
    struct ArtData
    {
        std::string data;
        std::string extension;

        bool operator==(const ArtData& other) const
        {
            return ((other.data == data) && (other.extension == extension));
        }

        bool operator!=(const ArtData& other) const
        {
            return !(other == *this);
        }
    };

    Metadata() = default;
    Metadata(const json& j);

    /// https://www.musicpd.org/doc/html/protocol.html#tags
    /// the duration of the song
    std::optional<float> duration;
    /// the artist name. Its meaning is not well-defined; see “composer” and “performer” for more specific tags.
    std::optional<std::vector<std::string>> artist;
    /// same as artist, but for sorting. This usually omits prefixes such as “The”.
    std::optional<std::vector<std::string>> artist_sort;
    /// the album name.
    std::optional<std::string> album;
    /// same as album, but for sorting.
    std::optional<std::string> album_sort;
    /// on multi-artist albums, this is the artist name which shall be used for the whole album. The exact meaning of this tag is not well-defined.
    std::optional<std::vector<std::string>> album_artist;
    /// same as albumartist, but for sorting.
    std::optional<std::vector<std::string>> album_artist_sort;
    /// a name for this song. This is not the song title. The exact meaning of this tag is not well-defined. It is often used by badly configured internet radio
    /// stations with broken tags to squeeze both the artist name and the song title in one tag.
    std::optional<std::string> name;
    /// the song’s release date. This is usually a 4-digit year.
    std::optional<std::string> date;
    /// the song’s original release date.
    std::optional<std::string> original_date;
    /// the artist who performed the song.
    std::optional<std::string> performer;
    /// the conductor who conducted the song.
    std::optional<std::string> conductor;
    /// “a work is a distinct intellectual or artistic creation, which can be expressed in the form of one or more audio recordings”
    std::optional<std::string> work;
    /// “used if the sound belongs to a larger category of sounds/music” (from the IDv2.4.0 TIT1 description).
    std::optional<std::string> grouping;
    /// the name of the label or publisher.
    std::optional<std::string> label;
    /// the artist id in the MusicBrainz database.
    std::optional<std::string> musicbrainz_artist_id;
    /// the album id in the MusicBrainz database.
    std::optional<std::string> musicbrainz_album_id;
    /// the album artist id in the MusicBrainz database.
    std::optional<std::string> musicbrainz_album_artist_id;
    /// the track id in the MusicBrainz database.
    std::optional<std::string> musicbrainz_track_id;
    /// the release track id in the MusicBrainz database.
    std::optional<std::string> musicbrainz_release_track_id;
    /// the work id in the MusicBrainz database.
    std::optional<std::string> musicbrainz_work_id;

    /// https://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata/
    /// A unique identity for this track within the context of an MPRIS object (eg: tracklist).
    std::optional<std::string> track_id;
    /// URI: The location of an image representing the track or album. Clients should not assume this will continue to exist when the media player stops giving
    /// out the URL
    std::optional<std::string> art_url;
    /// Base64 encoded data of an image representing the track or album
    std::optional<ArtData> art_data;
    /// The track lyrics
    std::optional<std::string> lyrics;
    /// The speed of the music, in beats per minute
    std::optional<uint16_t> bpm;
    /// Float: An automatically-generated rating, based on things such as how often it has been played. This should be in the range 0.0 to 1.0
    std::optional<float> auto_rating;
    /// A (list of) freeform comment(s)
    std::optional<std::vector<std::string>> comment;
    /// The composer(s) of the track
    std::optional<std::vector<std::string>> composer;
    /// Date/Time: When the track was created. Usually only the year component will be useful
    std::optional<std::string> content_created;
    /// Integer: The disc number on the album that this track is from
    std::optional<uint16_t> disc_number;
    /// Date/Time: When the track was first played
    std::optional<std::string> first_used;
    /// List of Strings: The genre(s) of the track
    std::optional<std::vector<std::string>> genre;
    /// Date/Time: When the track was last played
    std::optional<std::string> last_used;
    /// List of Strings: The lyricist(s) of the track
    std::optional<std::vector<std::string>> lyricist;
    /// String: The track title
    std::optional<std::string> title;
    /// Integer: The track number on the album disc
    std::optional<uint16_t> track_number;
    /// URI: The location of the media file.
    std::optional<std::string> url;
    /// Integer: The number of times the track has been played.
    std::optional<uint16_t> use_count;
    /// Float: A user-specified rating. This should be in the range 0.0 to 1.0.
    std::optional<float> user_rating;

    /// Spotify artist id
    std::optional<std::string> spotify_artist_id;
    /// Spotify track id
    std::optional<std::string> spotify_track_id;

    json toJson() const;
    void fromJson(const json& j);
    bool operator==(const Metadata& other) const;
};


#endif
