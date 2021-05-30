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

#ifndef METATAGS_HPP
#define METATAGS_HPP

#include <boost/optional.hpp>
#include <string>

#include "common/aixlog.hpp"
#include "common/json.hpp"

using json = nlohmann::json;

class Metatags
{
public:
    Metatags() = default;
    Metatags(const json& j)
    {
        fromJson(j);
    }

    /// https://www.musicpd.org/doc/html/protocol.html#tags
    /// the song file.
    boost::optional<std::string> file;
    /// the duration of the song
    boost::optional<float> duration;
    /// the artist name. Its meaning is not well-defined; see “composer” and “performer” for more specific tags.
    boost::optional<std::vector<std::string>> artist;
    /// same as artist, but for sorting. This usually omits prefixes such as “The”.
    boost::optional<std::vector<std::string>> artist_sort;
    /// the album name.
    boost::optional<std::string> album;
    /// same as album, but for sorting.
    boost::optional<std::string> album_sort;
    /// on multi-artist albums, this is the artist name which shall be used for the whole album. The exact meaning of this tag is not well-defined.
    boost::optional<std::vector<std::string>> album_artist;
    /// same as albumartist, but for sorting.
    boost::optional<std::vector<std::string>> album_artist_sort;
    /// the song title.
    // boost::optional<std::string> title;
    /// the decimal track number within the album.
    // boost::optional<uint16_t> track;
    /// a name for this song. This is not the song title. The exact meaning of this tag is not well-defined. It is often used by badly configured internet radio
    /// stations with broken tags to squeeze both the artist name and the song title in one tag.
    boost::optional<std::string> name;
    /// the music genre.
    // boost::optional<std::string> genre;
    /// the song’s release date. This is usually a 4-digit year.
    boost::optional<std::string> date;
    /// the song’s original release date.
    boost::optional<std::string> original_date;
    /// the artist who composed the song.
    // boost::optional<std::string> composer;
    /// the artist who performed the song.
    boost::optional<std::string> performer;
    /// the conductor who conducted the song.
    boost::optional<std::string> conductor;
    /// “a work is a distinct intellectual or artistic creation, which can be expressed in the form of one or more audio recordings”
    boost::optional<std::string> work;
    /// “used if the sound belongs to a larger category of sounds/music” (from the IDv2.4.0 TIT1 description).
    boost::optional<std::string> grouping;
    /// a human-readable comment about this song. The exact meaning of this tag is not well-defined.
    // boost::optional<std::string> comment;
    /// the decimal disc number in a multi-disc album.
    // boost::optional<uint16_t> disc;
    /// the name of the label or publisher.
    boost::optional<std::string> label;
    /// the artist id in the MusicBrainz database.
    boost::optional<std::string> musicbrainz_artist_id;
    /// the album id in the MusicBrainz database.
    boost::optional<std::string> musicbrainz_album_id;
    /// the album artist id in the MusicBrainz database.
    boost::optional<std::string> musicbrainz_album_artist_id;
    /// the track id in the MusicBrainz database.
    boost::optional<std::string> musicbrainz_track_id;
    /// the release track id in the MusicBrainz database.
    boost::optional<std::string> musicbrainz_release_track_id;
    /// the work id in the MusicBrainz database.
    boost::optional<std::string> musicbrainz_work_id;

    /// https://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata/
    /// A unique identity for this track within the context of an MPRIS object (eg: tracklist).
    boost::optional<std::string> track_id;
    /// URI: The location of an image representing the track or album. Clients should not assume this will continue to exist when the media player stops giving
    /// out the URL
    boost::optional<std::string> art_url;
    /// The track lyrics
    boost::optional<std::string> lyrics;
    /// The speed of the music, in beats per minute
    boost::optional<uint16_t> bpm;
    /// Float: An automatically-generated rating, based on things such as how often it has been played. This should be in the range 0.0 to 1.0
    boost::optional<float> auto_rating;
    /// A (list of) freeform comment(s)
    boost::optional<std::vector<std::string>> comment;
    /// The composer(s) of the track
    boost::optional<std::vector<std::string>> composer;
    /// Date/Time: When the track was created. Usually only the year component will be useful
    boost::optional<std::string> content_created;
    /// Integer: The disc number on the album that this track is from
    boost::optional<uint16_t> disc_number;
    /// Date/Time: When the track was first played
    boost::optional<std::string> first_used;
    /// List of Strings: The genre(s) of the track
    boost::optional<std::vector<std::string>> genre;
    /// Date/Time: When the track was last played
    boost::optional<std::string> last_used;
    /// List of Strings: The lyricist(s) of the track
    boost::optional<std::vector<std::string>> lyricist;
    /// String: The track title
    boost::optional<std::string> title;
    /// Integer: The track number on the album disc
    boost::optional<uint16_t> track_number;
    /// URI: The location of the media file.
    boost::optional<std::string> url;
    /// Integer: The number of times the track has been played.
    boost::optional<uint16_t> use_count;
    /// Float: A user-specified rating. This should be in the range 0.0 to 1.0.
    boost::optional<float> user_rating;

    /// Spotify artist id
    boost::optional<std::string> spotify_artist_id;
    /// Spotify track id
    boost::optional<std::string> spotify_track_id;

    json toJson() const
    {
        json j;
        addTag(j, "trackId", track_id);
        addTag(j, "file", file);
        addTag(j, "duration", duration);
        addTag(j, "artist", artist);
        addTag(j, "artistSort", artist_sort);
        addTag(j, "album", album);
        addTag(j, "albumSort", album_sort);
        addTag(j, "albumArtist", album_artist);
        addTag(j, "albumArtistSort", album_artist_sort);
        addTag(j, "title", title);
        addTag(j, "name", name);
        addTag(j, "genre", genre);
        addTag(j, "date", date);
        addTag(j, "originalDate", original_date);
        addTag(j, "composer", composer);
        addTag(j, "performer", performer);
        addTag(j, "conductor", conductor);
        addTag(j, "work", work);
        addTag(j, "grouping", grouping);
        addTag(j, "comment", comment);
        addTag(j, "discNumber", disc_number);
        addTag(j, "label", label);
        addTag(j, "musicbrainzArtistId", musicbrainz_artist_id);
        addTag(j, "musicbrainzAlbumId", musicbrainz_album_id);
        addTag(j, "musicbrainzAlbumArtistId", musicbrainz_album_artist_id);
        addTag(j, "musicbrainzTrackId", musicbrainz_track_id);
        addTag(j, "musicbrainzReleaseTrackId", musicbrainz_release_track_id);
        addTag(j, "musicbrainzWorkId", musicbrainz_work_id);
        addTag(j, "art_url", art_url);
        addTag(j, "lyrics", lyrics);
        addTag(j, "bpm", bpm);
        addTag(j, "autoRating", auto_rating);
        addTag(j, "comment", comment);
        addTag(j, "composer", composer);
        addTag(j, "contentCreated", content_created);
        addTag(j, "discNumber", disc_number);
        addTag(j, "firstUsed", first_used);
        addTag(j, "genre", genre);
        addTag(j, "lastUsed", last_used);
        addTag(j, "lyricist", lyricist);
        addTag(j, "title", title);
        addTag(j, "trackNumber", track_number);
        addTag(j, "url", url);
        addTag(j, "artUrl", art_url);
        addTag(j, "useCount", use_count);
        addTag(j, "userRating", user_rating);
        addTag(j, "spotifyArtistId", spotify_artist_id);
        addTag(j, "spotifyTrackId", spotify_track_id);
        return j;
    }

    void fromJson(const json& j)
    {
        readTag(j, "trackId", track_id);
        readTag(j, "file", file);
        readTag(j, "duration", duration);
        readTag(j, "artist", artist);
        readTag(j, "artistSort", artist_sort);
        readTag(j, "album", album);
        readTag(j, "albumSort", album_sort);
        readTag(j, "albumArtist", album_artist);
        readTag(j, "albumArtistSort", album_artist_sort);
        readTag(j, "title", title);
        readTag(j, "name", name);
        readTag(j, "genre", genre);
        readTag(j, "date", date);
        readTag(j, "originalDate", original_date);
        readTag(j, "composer", composer);
        readTag(j, "performer", performer);
        readTag(j, "conductor", conductor);
        readTag(j, "work", work);
        readTag(j, "grouping", grouping);
        readTag(j, "comment", comment);
        readTag(j, "discNumber", disc_number);
        readTag(j, "label", label);
        readTag(j, "musicbrainzArtistId", musicbrainz_artist_id);
        readTag(j, "musicbrainzAlbumId", musicbrainz_album_id);
        readTag(j, "musicbrainzAlbumArtistId", musicbrainz_album_artist_id);
        readTag(j, "musicbrainzTrackId", musicbrainz_track_id);
        readTag(j, "musicbrainzReleaseTrackId", musicbrainz_release_track_id);
        readTag(j, "musicbrainzWorkId", musicbrainz_work_id);
        readTag(j, "art_url", art_url);
        readTag(j, "lyrics", lyrics);
        readTag(j, "bpm", bpm);
        readTag(j, "autoRating", auto_rating);
        readTag(j, "comment", comment);
        readTag(j, "composer", composer);
        readTag(j, "contentCreated", content_created);
        readTag(j, "discNumber", disc_number);
        readTag(j, "firstUsed", first_used);
        readTag(j, "genre", genre);
        readTag(j, "lastUsed", last_used);
        readTag(j, "lyricist", lyricist);
        readTag(j, "title", title);
        readTag(j, "trackNumber", track_number);
        readTag(j, "url", url);
        readTag(j, "artUrl", art_url);
        readTag(j, "useCount", use_count);
        readTag(j, "userRating", user_rating);
        readTag(j, "spotifyArtistId", spotify_artist_id);
        readTag(j, "spotifyTrackId", spotify_track_id);
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
            LOG(ERROR) << "failed to read tag: '" << tag << "': " << e.what() << '\n';
        }
    }

    template <typename T>
    void addTag(json& j, const std::string& tag, const boost::optional<T>& source) const
    {
        try
        {
            if (!source.has_value())
            {
                if (j.contains(tag))
                    j.erase(tag);
            }
            else
                j[tag] = source.value();
        }
        catch (const std::exception& e)
        {
            LOG(ERROR) << "failed to add tag: '" << tag << "': " << e.what() << '\n';
        }
    }
};


#endif
