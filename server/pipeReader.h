/***
    This file is part of snapcast
    Copyright (C) 2015  Johannes Pohl

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

#ifndef PIPE_READER_H
#define PIPE_READER_H

#include <thread>
#include <atomic>
#include <string>
#include "encoder/encoder.h"
#include "message/sampleFormat.h"
#include "message/header.h"


class PipeReader;


/// Callback interface for users of PipeReader
/**
 * Users of PipeReader should implement this to get the data
 */
class PipeListener
{
public:
	virtual void onChunkRead(const PipeReader* pipeReader, const msg::PcmChunk* chunk, double duration) = 0;
	virtual void onResync(const PipeReader* pipeReader, double ms) = 0;
};



/// Reads and decodes PCM data from a named pipe
/**
 * Reads PCM from a named pipe and passed the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PipeListener
 */
class PipeReader : public EncoderListener
{
public:
	/// ctor. Encoded PCM data is passed to the PipeListener
	PipeReader(PipeListener* pipeListener, const msg::SampleFormat& sampleFormat, const std::string& codec, const std::string& fifoName, size_t pcmReadMs = 20);
	virtual ~PipeReader();

	void start();
	void stop();

	/// Implementation of EncoderListener::onChunkEncoded
	virtual void onChunkEncoded(const Encoder* encoder, msg::PcmChunk* chunk, double duration);
	msg::Header* getHeader();

protected:
	void worker();
	int fd_;
	timeval tvEncodedChunk_;
	std::atomic<bool> active_;
	std::thread readerThread_;
	PipeListener* pipeListener_;
	msg::SampleFormat sampleFormat_;
	size_t pcmReadMs_;
	std::unique_ptr<Encoder> encoder_;
};


#endif
