/***
    This file is part of snapcast
    Copyright (C) 2014-2016  Johannes Pohl

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

#ifndef FILE_STREAM_H
#define FILE_STREAM_H

#include "pcmStream.h"
#include <fstream>


/// Reads and decodes PCM data from a file
/**
 * Reads PCM from a file and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class FileStream : public PcmStream
{
public:
	/// ctor. Encoded PCM data is passed to the PipeListener
	FileStream(PcmListener* pcmListener, const StreamUri& uri);
	virtual ~FileStream();

protected:
	virtual void worker();
	std::ifstream ifs;
};


#endif
