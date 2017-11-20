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


namespace msg
{

class StreamTags : public JsonMessage
{
public:
	StreamTags() : JsonMessage(message_type::kStreamTags)
	{
		msg["meta_artist"] = "";
		msg["meta_album"]  = "";
		msg["meta_track"]  = "";
	}

	virtual ~StreamTags()
	{
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
};

}


#endif


