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

#ifndef PROCESS_STREAM_H
#define PROCESS_STREAM_H

#include <memory>
#include <string>

#include "pcmStream.h"
#include "process.hpp"


/// Starts an external process and reads and PCM data from stdout
/**
 * Starts an external process, reads PCM data from stdout, and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class ProcessStream : public PcmStream
{
public:
	/// ctor. Encoded PCM data is passed to the PipeListener
	ProcessStream(PcmListener* pcmListener, const StreamUri& uri);
	virtual ~ProcessStream();

	virtual void start();
	virtual void stop();

protected:
	std::string exe_;
	std::string path_;
	std::string params_;
	std::unique_ptr<Process> process_;
	std::thread stderrReaderThread_;
	bool logStderr_;

	virtual void worker();
	virtual void stderrReader();
	virtual void onStderrMsg(const char* buffer, size_t n);
	virtual void initExeAndPath(const std::string& filename);

	bool fileExists(const std::string& filename);
	std::string findExe(const std::string& filename);
};


#endif
