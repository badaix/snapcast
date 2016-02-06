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

#ifndef PCM_READER_H
#define PCM_READER_H

#include <thread>
#include <atomic>
#include <string>
#include <map>
#include "readerUri.h"
#include "../encoder/encoder.h"
#include "message/sampleFormat.h"
#include "message/header.h"


class PcmReader;


/// Callback interface for users of PcmReader
/**
 * Users of PcmReader should implement this to get the data
 */
class PcmListener
{
public:
	virtual void onChunkRead(const PcmReader* pcmReader, const msg::PcmChunk* chunk, double duration) = 0;
	virtual void onResync(const PcmReader* pcmReader, double ms) = 0;
};


/// Reads and decodes PCM data
/**
 * Reads PCM and passes the data to an encoder.
 * Implements EncoderListener to get the encoded data.
 * Data is passed to the PcmListener
 */
class PcmReader : public EncoderListener
{
public:
	/// ctor. Encoded PCM data is passed to the PcmListener
	PcmReader(PcmListener* pcmListener, const ReaderUri& uri);
	virtual ~PcmReader();

	virtual void start();
	virtual void stop();

	/// Implementation of EncoderListener::onChunkEncoded
	virtual void onChunkEncoded(const Encoder* encoder, msg::PcmChunk* chunk, double duration);
	virtual std::shared_ptr<msg::Header> getHeader();

	virtual const ReaderUri& getUri() const;
	virtual const std::string& getName() const;
	virtual const SampleFormat& getSampleFormat() const;

protected:
	virtual void worker() = 0;
	timeval tvEncodedChunk_;
	std::atomic<bool> active_;
	std::thread readerThread_;
	PcmListener* pcmListener_;
	ReaderUri uri_;
	SampleFormat sampleFormat_;
	size_t pcmReadMs_;
	std::unique_ptr<Encoder> encoder_;
	std::string name_;
};


#endif
