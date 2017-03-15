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

#include "encoderFactory.h"
#include "pcmEncoder.h"
#if defined(HAS_OGG) && defined(HAS_VORBIS) && defined(HAS_VORBIS_ENC)
#include "oggEncoder.h"
#endif
#if defined(HAS_FLAC)
#include "flacEncoder.h"
#endif
#include "common/utils.h"
#include "common/snapException.h"
#include "common/log.h"


using namespace std;


Encoder* EncoderFactory::createEncoder(const std::string& codecSettings) const
{
	Encoder* encoder;
	std::string codec(codecSettings);
	std::string codecOptions;
	if (codec.find(":") != std::string::npos)
	{
		codecOptions = trim_copy(codec.substr(codec.find(":") + 1));
		codec = trim_copy(codec.substr(0, codec.find(":")));
	}
    if (codec == "pcm")
        encoder = new PcmEncoder(codecOptions);
#if defined(HAS_OGG) && defined(HAS_VORBIS) && defined(HAS_VORBIS_ENC)
    else if (codec == "ogg")
		encoder = new OggEncoder(codecOptions);
#endif
#if defined(HAS_FLAC)
	else if (codec == "flac")
		encoder = new FlacEncoder(codecOptions);
#endif
	else
	{
		throw SnapException("unknown codec: " + codec);
	}

	return encoder;
/*	try
	{
		encoder->init(NULL, format, codecOptions);
	}
	catch (const std::exception& e)
	{
		cout << "Error: " << e.what() << "\n";
		return 1;
	}
*/
}



