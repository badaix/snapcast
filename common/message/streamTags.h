/***
    This file is part of snapcast
    Copyright (C) 2014-2018  Johannes Pohl

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
	/* 
	Usage:
		json jtag = {
			{"artist", "Pink Floyd"},
			{"album", "Dark Side of the Moon"},
               		{"track", "Money"},
			{"spotifyid", "akjhasi7wehke7698"},
			{"musicbrainzid", "akjhasi7wehke7698"},
		};
		this->meta_.reset(new msg::StreamTags(jtag));

		Stream input can decide on tags, IDK... but smart
		to use some common naming scheme
	*/

	StreamTags(json j) : JsonMessage(message_type::kStreamTags)
	{
		msg = j;
	}

	StreamTags() : JsonMessage(message_type::kStreamTags)
	{
	}

	virtual ~StreamTags()
	{
	}
};

}


#endif


