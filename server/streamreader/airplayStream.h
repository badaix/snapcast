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

#ifndef AIRPLAY_STREAM_H
#define AIRPLAY_STREAM_H

#include "processStream.h"

/*
 * Expat is used in metadata parsing from Shairport-sync.
 * Without HAS_EXPAT defined no parsing will occur.
 */
#ifdef HAS_EXPAT
#include <expat.h>
#endif

class TageEntry
{
public:
        TageEntry(): isBase64(false), length(0) {}

        std::string code;
        std::string type;
        std::string data;
        bool isBase64;
        int length;
};

/// Starts shairport-sync and reads PCM data from stdout

/**
 * Starts librespot, reads PCM data from stdout, and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 * usage:
 *   snapserver -s "airplay:///shairport-sync?name=Airplay[&devicename=Snapcast][&port=5000]"
 */
class AirplayStream : public ProcessStream
{
public:
	/// ctor. Encoded PCM data is passed to the PipeListener
	AirplayStream(PcmListener* pcmListener, const StreamUri& uri);
	virtual ~AirplayStream();

protected:
#ifdef HAS_EXPAT
	XML_Parser parser_;
#endif
	std::unique_ptr<TageEntry> entry_;
	std::string buf_;
	json jtag_;

	void pipeReader();
#ifdef HAS_EXPAT
	int parse(std::string line);
	void createParser();
	void push();
#endif

	virtual void onStderrMsg(const char* buffer, size_t n);
	virtual void initExeAndPath(const std::string& filename);
	size_t port_;
	std::string pipePath_;
	std::string params_wo_port_;
	std::thread pipeReaderThread_;

#ifdef HAS_EXPAT
	static void XMLCALL element_start(void *userdata, const char *element_name, const char **attr);
	static void XMLCALL element_end(void *userdata, const char *element_name);
	static void XMLCALL data(void *userdata, const char *content, int length);
#endif
};


#endif
