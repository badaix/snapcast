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

#include "metadata.hpp"

static constexpr auto LOG_TAG = "Metadata";


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
void addTag(json& j, const std::string& tag, const std::optional<T>& source)
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
        LOG(ERROR, LOG_TAG) << "failed to add tag: '" << tag << "': " << e.what() << '\n';
    }
}
} // namespace


Metadata::Metadata(const json& j)
{
    fromJson(j);
}


json Metadata::toJson() const
{
    json j(json::object());
    addTag(j, "trackId", track_id);
    addTag(j, "duration", duration);
    addTag(j, "artist", artist);
    addTag(j, "artistSort", artist_sort);
    addTag(j, "album", album);
    addTag(j, "albumSort", album_sort);
    addTag(j, "albumArtist", album_artist);
    addTag(j, "albumArtistSort", album_artist_sort);
    addTag(j, "name", name);
    addTag(j, "date", date);
    addTag(j, "originalDate", original_date);
    addTag(j, "performer", performer);
    addTag(j, "conductor", conductor);
    addTag(j, "work", work);
    addTag(j, "grouping", grouping);
    addTag(j, "label", label);
    addTag(j, "musicbrainzArtistId", musicbrainz_artist_id);
    addTag(j, "musicbrainzAlbumId", musicbrainz_album_id);
    addTag(j, "musicbrainzAlbumArtistId", musicbrainz_album_artist_id);
    addTag(j, "musicbrainzTrackId", musicbrainz_track_id);
    addTag(j, "musicbrainzReleaseTrackId", musicbrainz_release_track_id);
    addTag(j, "musicbrainzWorkId", musicbrainz_work_id);
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
    if (art_data.has_value())
    {
        j["artData"] = {{"data", art_data->data}, {"extension", art_data->extension}};
    }
    addTag(j, "useCount", use_count);
    addTag(j, "userRating", user_rating);
    addTag(j, "spotifyArtistId", spotify_artist_id);
    addTag(j, "spotifyTrackId", spotify_track_id);
    return j;
}


void Metadata::fromJson(const json& j)
{
    static std::set<std::string> supported_tags = {"trackId",
                                                   "duration",
                                                   "artist",
                                                   "artistSort",
                                                   "album",
                                                   "albumSort",
                                                   "albumArtist",
                                                   "albumArtistSort",
                                                   "name",
                                                   "date",
                                                   "originalDate",
                                                   "performer",
                                                   "conductor",
                                                   "work",
                                                   "grouping",
                                                   "label",
                                                   "musicbrainzArtistId",
                                                   "musicbrainzAlbumId",
                                                   "musicbrainzAlbumArtistId",
                                                   "musicbrainzTrackId",
                                                   "musicbrainzReleaseTrackId",
                                                   "musicbrainzWorkId",
                                                   "lyrics",
                                                   "bpm",
                                                   "autoRating",
                                                   "comment",
                                                   "composer",
                                                   "contentCreated",
                                                   "discNumber",
                                                   "firstUsed",
                                                   "genre",
                                                   "lastUsed",
                                                   "lyricist",
                                                   "title",
                                                   "trackNumber",
                                                   "url",
                                                   "artUrl",
                                                   "artData",
                                                   "useCount",
                                                   "userRating",
                                                   "spotifyArtistId",
                                                   "spotifyTrackId"};
    for (const auto& element : j.items())
    {
        if (supported_tags.find(element.key()) == supported_tags.end())
            LOG(WARNING, LOG_TAG) << "Tag not supoorted: " << element.key() << "\n";
    }

    readTag(j, "trackId", track_id);
    readTag(j, "duration", duration);
    readTag(j, "artist", artist);
    readTag(j, "artistSort", artist_sort);
    readTag(j, "album", album);
    readTag(j, "albumSort", album_sort);
    readTag(j, "albumArtist", album_artist);
    readTag(j, "albumArtistSort", album_artist_sort);
    readTag(j, "name", name);
    readTag(j, "date", date);
    readTag(j, "originalDate", original_date);
    readTag(j, "performer", performer);
    readTag(j, "conductor", conductor);
    readTag(j, "work", work);
    readTag(j, "grouping", grouping);
    readTag(j, "label", label);
    readTag(j, "musicbrainzArtistId", musicbrainz_artist_id);
    readTag(j, "musicbrainzAlbumId", musicbrainz_album_id);
    readTag(j, "musicbrainzAlbumArtistId", musicbrainz_album_artist_id);
    readTag(j, "musicbrainzTrackId", musicbrainz_track_id);
    readTag(j, "musicbrainzReleaseTrackId", musicbrainz_release_track_id);
    readTag(j, "musicbrainzWorkId", musicbrainz_work_id);
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
    art_data = std::nullopt;
    if (j.contains("artData") && j["artData"].contains("data") && j["artData"].contains("extension"))
    {
        art_data = ArtData{j["artData"]["data"].get<std::string>(), j["artData"]["extension"].get<std::string>()};
    }
    readTag(j, "useCount", use_count);
    readTag(j, "userRating", user_rating);
    readTag(j, "spotifyArtistId", spotify_artist_id);
    readTag(j, "spotifyTrackId", spotify_track_id);
}


bool Metadata::operator==(const Metadata& other) const
{
    // expensive, but not called often and less typing
    return (toJson() == other.toJson());
}
