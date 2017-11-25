/***
    This file is part of snapcast
    Copyright (C) 2014-2017  Johannes Pohl

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

#ifndef STREAMTAGS_H
#define STREAMTAGS_H

#include "jsonMessage.h"

/*
 * Due to the PCM pipe implementation of snapcast input we cannot know track start/end
 * it's all a long stream (although we detect idle situations)
 * 
 * So, we cannot push metadata on start of track as we don't know when that is.
 * 
 * I.E. we push metadata as we get an update, as we don't know when an update
 * is complete (different meta supported in different stream interfaces)
 * it is the streamreaders responsibility to update metadata and 
 * trigger a client notification.
 *
 * I.E. we need to suppply the client notification mechanism.
 */

namespace msg
{

class StreamTags : public JsonMessage
{
public:
	StreamTags() : JsonMessage(message_type::kStreamTags)
	{
		msg["meta_artist"]    = "";
		msg["meta_album"]     = "";
		msg["meta_track"]     = "";
		msg["meta_albumart"]  = "";
	}

	virtual ~StreamTags()
	{
	}

	json toJson() const
	{
		json j = {
			{"artist", getArtist()},
			{"album", getAlbum()},
			{"track", getTrack()},
		};
		return j;
	}

	std::string getArtist() const
	{
		return msg["meta_artist"];
	}

	std::string getAlbum() const
	{
		return msg["meta_album"];
	}

	std::string getTrack() const
	{
		return msg["meta_track"];
	}

	std::string getAlbumArt() const
	{
		return msg["meta_albumart"];
	}

	void setArtist(std::string artist)
	{
		msg["meta_artist"] = artist;
	}

	void setAlbum(std::string album)
	{
		msg["meta_album"] = album;
	}

	void setTrack(std::string track)
	{
		msg["meta_track"] = track;
	}

	// Ascii encoded image XXX: more details
	void setAlbumArt(std::string art)
	{
		msg["meta_albumart"] = art;
	}
};

}


#endif


